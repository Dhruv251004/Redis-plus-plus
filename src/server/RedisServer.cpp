#include <cerrno>
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
#include "database/Database.h"
#include "utility/StringUtils.h"
#include "utility/RedisPlusPlus.h"
#include "constants/Constants.h"

using namespace std;

constexpr int MAX_EVENTS = 64;

const unordered_set<string> RedisServer::write_command_ = {"SET", "DEL", "EXPIRE", "EXPIREAT", "PEXPIREAT", "PERSIST"};

RedisServer::RedisServer(int port) : tcp_server_(port), db_(), aof_(BASE_DB_PATH + "/" + AOF_FILE_NAME), command_executor_(db_, aof_), rdb_(BASE_DB_PATH + "/" + RDB_FILE_NAME) {}

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

    while (true)
    {
        int num_events = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
        if (num_events == -1)
        {
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
                handle_client(current_fd);
            }
        }
    }
}

void RedisServer::handle_client(int client_fd)
{
    Connection &conn = connection_manager_.get(client_fd);
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
                spdlog::warn("[RedisServer.cpp::RedisServer::handle_client] invalid request from client fd {} ({}), closing connection", client_fd, input_resp->data());
                string err = RespSerializer::serialize(*input_resp);
                tcp_server_.send_message(client_fd, err);
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
                tcp_server_.close_client(client_fd);
                connection_manager_.remove(client_fd);
                return;
            }

            // persist in memeory
            RespValue output_resp = command_executor_.execute(*input_resp);

            // persist in db
            persist_in_db(*input_resp, output_resp);
            string output_msg = RespSerializer::serialize(output_resp);
            conn.consume_read_buffer(consumed);

            if (!tcp_server_.send_message(client_fd, output_msg))
            {
                spdlog::warn("[RedisServer.cpp::RedisServer::handle_client] failed to send reply to client fd {}, closing connection", client_fd);
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
                tcp_server_.close_client(client_fd);
                connection_manager_.remove(client_fd);
                return;
            }
        }

        return;
    }

    if (bytes == 0)
    {
        spdlog::info("[RedisServer.cpp::RedisServer::handle_client] client disconnected: fd {}", client_fd);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        tcp_server_.close_client(client_fd);
        connection_manager_.remove(client_fd);
        return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return;
    }
    spdlog::error("[RedisServer.cpp::RedisServer::handle_client] read error on client fd {}: {}", client_fd, strerror(errno));
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
    tcp_server_.close_client(client_fd);
    connection_manager_.remove(client_fd);
}

void RedisServer::handle_replay(string_view cmds)
{
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

        command_executor_.execute(*input_resp);
        cmds.remove_prefix(consumed);
    }
}

bool RedisServer::is_write_command(const string &cmd)
{
    return write_command_.find(cmd) != write_command_.end();
}

void RedisServer::stop()
{
    tcp_server_.stop();
}

void RedisServer::persist_in_db(const RespValue &input, const RespValue &output)
{
    if (output.type() == RespValue::Type::Error)
        return;

    const auto &args = input.children();
    string cmdName = uppercase(args[0].data());
    if (!is_write_command(cmdName))
        return;

    string entry;
    if (cmdName == "EXPIRE" || (cmdName == "SET" && args.size() > 3))
    {
        if (cmdName == "SET")
            entry = RespSerializer::serialize(RespValue::array(
                {RespValue::bulk_string("SET"), args[1], args[2]}));

        auto deadline = db_.ttl(args[1].data());
        if (deadline.has_value() && *deadline != NO_EXPIRY)
            entry += RespSerializer::serialize(RespValue::array(
                {RespValue::bulk_string("PEXPIREAT"), args[1],
                 RespValue::bulk_string(to_string(*deadline))}));
    }
    else
    {
        entry = RespSerializer::serialize(input);
    }

    if (!aof_.append(entry))
        spdlog::error("[RedisServer.cpp::RedisServer::persist_in_db] failed to append {} command to aof, it will be lost on restart", cmdName);
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