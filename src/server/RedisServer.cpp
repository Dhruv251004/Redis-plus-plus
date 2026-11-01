#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include "RedisServer.h"
#include "protocol/RespParser.h"
#include "command/CommandExecutor.h"
#include "protocol/RespSerializer.h"
#include "protocol/ParseResult.h"
#include "persistence/AofEntry.h"
#include "database/Database.h"
#include "utility/StringUtils.h"
#include "utility/RedisPlusPlus.h"
#include "constants/Constants.h"

using namespace std;

constexpr int MAX_EVENTS = 64;
constexpr int EXPIRE_CYCLE_MS = 100;
constexpr size_t EXPIRE_BATCH = 1000;
constexpr auto EXPIRE_TIME_BUDGET = chrono::milliseconds(5);

RedisServer::RedisServer(int port) : tcp_server_(port), db_(), aof_(BASE_DB_PATH + "/" + AOF_FILE_NAME), pub_sub_manager_(connection_manager_), command_executor_(db_, pub_sub_manager_), rdb_(BASE_DB_PATH + "/" + RDB_FILE_NAME)
{
    connection_manager_.set_notify_handler([this](int client_fd, Connection &conn)
                                           { handle_notify_event(client_fd, conn); });
}

void RedisServer::start()
{
    bool res = tcp_server_.start();
    if (!res)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to start tcp server, aborting startup: {}", strerror(errno));
        return;
    }

    if (!rdb_.read(db_))
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to restore db snapshot from {}, aborting startup", BASE_DB_PATH + "/" + RDB_FILE_NAME);
        return;
    }

    spdlog::info("[RedisServer.cpp::RedisServer::start] db snapshot restored, {} keys loaded", db_.get_db_size());

    if (!aof_.open())
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to open aof file {}, aborting startup", BASE_DB_PATH + "/" + AOF_FILE_NAME);
        return;
    }

    // replay all messages
    aof_.replay([this](string_view cmd)
                { handle_replay(cmd); });
    spdlog::info("[RedisServer.cpp::RedisServer::start] aof replay finished, {} keys in db", db_.get_db_size());

    // installed after replay so replayed commands aren't logged again
    command_executor_.set_write_observer([this](const RespValue &command, const RespValue &)
                                         { persist_in_db(command); });

    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to create epoll instance: {}", strerror(errno));
        return;
    }

    // Register listening socket
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tcp_server_.fd();

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tcp_server_.fd(), &ev) == -1)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to add server socket to epoll: {}", strerror(errno));
        return;
    }

    if (!add_timer())
    {
        spdlog::error("[RedisServer.cpp::RedisServer::start] failed to add timer for snapshot");
        return;
    }

    printWelcomeScreen();
    spdlog::info("[RedisServer.cpp::RedisServer::start] server ready to accept connections");

    vector<epoll_event> events(MAX_EVENTS);
    auto last_expire_cycle = chrono::steady_clock::now();

    while (true)
    {
        int num_events = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, EXPIRE_CYCLE_MS);
        if (num_events == -1)
        {
            if (errno == EINTR)
                continue;
            spdlog::error("[RedisServer.cpp::RedisServer::start] epoll_wait error: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < num_events; i++)
        {
            int current_fd = events[i].data.fd;
            if (current_fd == tcp_server_.fd())
            {
                int client_fd = tcp_server_.accept_client();
                if (client_fd == -1)
                {
                    spdlog::warn("[RedisServer.cpp::RedisServer::start] failed to accept client: {}", strerror(errno));
                    continue;
                }

                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
                {
                    spdlog::error("[RedisServer.cpp::RedisServer::start] failed to add client fd {} to epoll: {}", client_fd, strerror(errno));
                    tcp_server_.close_client(client_fd); // FIX
                    continue;
                }
                connection_manager_.add(client_fd);
                spdlog::info("[RedisServer.cpp::RedisServer::start] new client connected: fd {}", client_fd);
            }
            else if (current_fd == timer_fd_)
            {
                uint64_t expirations;
                ssize_t n = ::read(timer_fd_, &expirations, sizeof(expirations));

                if (n == sizeof(expirations))
                {
                    if (!save_snapshot())
                        spdlog::error("[RedisServer.cpp::RedisServer::start] scheduled snapshot failed, will retry at the next interval");
                }
                else if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    spdlog::error("[RedisServer.cpp::RedisServer::start] failed to read snapshot timer: {}", strerror(errno));
                }
            }
            else
            {
                handle_client(current_fd, events[i].events);
            }
        }

        if (chrono::steady_clock::now() - last_expire_cycle >= chrono::milliseconds(EXPIRE_CYCLE_MS))
        {
            purge_expired_keys();
            last_expire_cycle = chrono::steady_clock::now();
        }
    }
}

void RedisServer::handle_client(int client_fd, uint32_t event)
{
    // Handle socket errors and disconnects first.
    if (event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
    {
        if (!close_client(client_fd))
        {
            spdlog::error("[RedisServer.cpp::RedisServer::handle_client] failed to completely close client fd {}", client_fd);
        }
        return;
    }

    // Obtain the connection only after handling early disconnects.
    Connection &conn = connection_manager_.get(client_fd);

    // 1. Read incoming data and queue responses.
    if (event & EPOLLIN)
    {
        if (!handle_read_client(client_fd, conn))
        {
            // The handler closed the connection or encountered a fatal error.
            return;
        }
    }

    if (conn.has_write_data())
    {
        if (!handle_write_client(client_fd, conn))
        {
            // The handler closed the connection or failed to update epoll.
            return;
        }
    }
}

bool RedisServer::handle_read_client(int client_fd, Connection &conn)
{
    // The connection is closing once its pending reply is flushed, so ignore further input.
    if (conn.close_after_write())
        return true;

    auto [bytes, msg] = tcp_server_.receive_message(client_fd);

    if (bytes > 0)
    {
        conn.append_read_data(msg);
        while (true)
        {
            auto [parse_status, input_resp, consumed] = RespParser::parse(conn.read_buffer());
            if (parse_status == ParseStatus::Incomplete)
                break;
            if (parse_status == ParseStatus::Invalid)
            {
                spdlog::warn("[RedisServer.cpp::RedisServer::handle_read_client] invalid request from client fd {} ({}), closing connection", client_fd, input_resp->data());
                string err = RespSerializer::serialize(*input_resp);

                conn.append_write_data(err);
                conn.set_close_after_write(true);
                return true;
            }

            RespValue output_resp = command_executor_.execute(*input_resp, conn);
            string output_msg = RespSerializer::serialize(output_resp);

            // remove from read and append to write
            conn.consume_read_buffer(consumed);
            conn.append_write_data(output_msg);
        }

        return true;
    }

    if (bytes == 0)
    {
        spdlog::info("[RedisServer.cpp::RedisServer::handle_read_client] client disconnected: fd {}", client_fd);
        // Handle connection error/disconnect.
        if (!close_client(client_fd))
            spdlog::error("[RedisServer.cpp::RedisServer::handle_read_client] cleanup encountered an error for fd {}", client_fd);

        return false;
    }

    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        return true;

    spdlog::error("[RedisServer.cpp::RedisServer::handle_read_client] read error on client fd {}: {}", client_fd, strerror(errno));
    // Handle connection error/disconnect.
    if (!close_client(client_fd))
        spdlog::error("[RedisServer.cpp::RedisServer::handle_read_client] cleanup encountered an error for fd {}", client_fd);

    return false;
}

bool RedisServer::handle_write_client(int client_fd, Connection &conn)
{
    while (conn.has_write_data())
    {
        string_view write_data = conn.write_buffer();
        ssize_t bytes_wrote = tcp_server_.send_message(client_fd, write_data);
        if (bytes_wrote > 0)
        {
            conn.consume_write_buffer(bytes_wrote);
            continue;
        }

        if (bytes_wrote == 0)
        {
            spdlog::warn("[RedisServer.cpp::RedisServer::handle_write_client] send returned 0 with pending data for fd {}, closing connection", client_fd);
            // Handle connection error/disconnect.
            if (!close_client(client_fd))
                spdlog::error("[RedisServer.cpp::RedisServer::handle_write_client] cleanup encountered an error for fd {}", client_fd);
            return false;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // While closing, stop watching EPOLLIN: unread input would keep waking the loop.
            uint32_t events = conn.close_after_write() ? EPOLLOUT : (EPOLLIN | EPOLLOUT);
            if (!update_event(client_fd, events))
            {
                if (!close_client(client_fd))
                    spdlog::error("[RedisServer.cpp::RedisServer::handle_write_client] cleanup encountered an error for fd {}", client_fd);
                return false;
            }
            return true;
        }
        else
        {
            spdlog::warn("[RedisServer.cpp::RedisServer::handle_write_client] failed to send reply to client fd {}, closing connection", client_fd);
            // Handle connection error/disconnect.
            if (!close_client(client_fd))
                spdlog::error("[RedisServer.cpp::RedisServer::handle_write_client] cleanup encountered an error for fd {}", client_fd);
            return false;
        }
    }
    if (!update_event(client_fd, EPOLLIN))
    {
        if (!close_client(client_fd))
            spdlog::error("[RedisServer.cpp::RedisServer::handle_write_client] cleanup encountered an error for fd {}", client_fd);
        return false;
    }

    if (conn.close_after_write())
    {
        if (!close_client(client_fd))
            spdlog::error("[RedisServer.cpp::RedisServer::handle_write_client] cleanup encountered an error for fd {}", client_fd);
        return false;
    }
    return true;
}

void RedisServer::handle_replay(string_view cmds)
{
    Connection replay_conn(-1);
    while (cmds.size() > 0)
    {
        auto [parse_status, input_resp, consumed] = RespParser::parse(cmds);
        if (parse_status == ParseStatus::Invalid)
        {
            spdlog::warn("[RedisServer.cpp::RedisServer::handle_replay] corrupted aof entry, skipping restore of the remaining {} bytes", cmds.size());
            return;
        }
        if (parse_status == ParseStatus::Incomplete)
        {
            spdlog::warn("[RedisServer.cpp::RedisServer::handle_replay] truncated aof entry, restore aborted with {} unread bytes", cmds.size());
            return;
        }

        command_executor_.execute(*input_resp, replay_conn);
        cmds.remove_prefix(consumed);
    }
}

void RedisServer::stop()
{
    tcp_server_.stop();
}

void RedisServer::persist_in_db(const RespValue &command)
{
    string entry = aof_entry_for(command, db_);
    if (!aof_.append(entry))
        spdlog::error("[RedisServer.cpp::RedisServer::persist_in_db] failed to append {} command to aof, it will be lost on restart", uppercase(command.children()[0].data()));
}

void RedisServer::purge_expired_keys()
{
    auto start = chrono::steady_clock::now();
    size_t total = 0;
    size_t removed;
    do
    {
        removed = db_.purge_expired(EXPIRE_BATCH);
        total += removed;
    } while (removed == EXPIRE_BATCH && chrono::steady_clock::now() - start < EXPIRE_TIME_BUDGET);

    if (total > 0)
        spdlog::debug("[RedisServer.cpp::RedisServer::purge_expired_keys] removed {} expired keys", total);
}

bool RedisServer::save_snapshot()
{
    spdlog::info("[RedisServer.cpp::RedisServer::save_snapshot] starting db snapshot");

    // sync aof to db now dump db snapshot to rdb then clear the aof file
    if (!aof_.fsync())
    {
        spdlog::error("[RedisServer.cpp::RedisServer::save_snapshot] snapshot aborted, aof fsync failed");
        return false;
    }

    if (!rdb_.write(db_))
    {
        spdlog::error("[RedisServer.cpp::RedisServer::save_snapshot] snapshot aborted, rdb write failed (aof left untouched)");
        return false;
    }

    if (!aof_.truncate())
    {
        spdlog::error("[RedisServer.cpp::RedisServer::save_snapshot] snapshot saved but aof truncate failed, aof will keep growing");
        return false;
    }

    spdlog::info("[RedisServer.cpp::RedisServer::save_snapshot] db snapshot completed, {} keys saved", db_.get_db_size());
    return true;
}

bool RedisServer::add_timer()
{
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd_ == -1)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::add_timer] timerfd_create failed: {}", strerror(errno));
        return false;
    }

    itimerspec timer{};
    timer.it_value.tv_sec = 300;
    timer.it_interval.tv_sec = 300;

    if (timerfd_settime(timer_fd_, 0, &timer, nullptr) == -1)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::add_timer] timerfd_settime failed: {}", strerror(errno));
        ::close(timer_fd_);
        timer_fd_ = -1;
        return false;
    }

    epoll_event ev{};
    ev.data.fd = timer_fd_;
    ev.events = EPOLLIN;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev) == -1)
    {
        spdlog::error("[RedisServer.cpp::RedisServer::add_timer] failed to add timer to epoll: {}", strerror(errno));
        ::close(timer_fd_);
        timer_fd_ = -1;
        return false;
    }

    return true;
}

bool RedisServer::update_event(int client_fd, uint32_t events)
{
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = client_fd;

    return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev) == 0;
}

bool RedisServer::close_client(int client_fd)
{
    bool success = true;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr) == -1)
    {
        if (errno != ENOENT && errno != EBADF)
        {
            spdlog::error("[RedisServer::close_client] epoll_ctl DEL failed for fd {}: {}", client_fd, strerror(errno));
            success = false;
        }
    }

    if (!tcp_server_.close_client(client_fd))
    {
        spdlog::error("[RedisServer::close_client] failed to close fd {}", client_fd);
        success = false;
    }

    // Drop the fd from every topic so publish never targets a closed (or reused) fd.
    pub_sub_manager_.remove_all_subscriptions(client_fd);
    connection_manager_.remove(client_fd);

    return success;
}

void RedisServer::handle_notify_event(int client_fd, Connection &conn)
{
    //  currently it only handles append to write buffer but later can be genralized to any sort of event
    handle_write_client(client_fd, conn);
}