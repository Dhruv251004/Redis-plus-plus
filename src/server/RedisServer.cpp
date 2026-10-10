#include <cerrno>
#include <iostream>
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
        return;

    if (!rdb_.read(db_))
    {
        cerr << "failed to restore db snapshot\n";
        return;
    }

    if (!aof_.open())
    {
        cerr << "failed to open aof file\n";
        return;
    }

    // replay all messages
    aof_.replay([this](string_view cmd)
                { handle_replay(cmd); });

    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1)
    {
        cerr << "Failed to create epoll instance\n";
        return;
    }

    // Register listening socket
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tcp_server_.fd();

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tcp_server_.fd(), &ev) == -1)
    {
        cerr << "Failed to add server socket to epoll\n";
        return;
    }

    if (!add_timer())
    {
        cerr << "failed to add timer for snapshot\n";
        return;
    }

    printWelcomeScreen();

    vector<epoll_event> events(MAX_EVENTS);

    while (true)
    {
        int num_events = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, -1);
        if (num_events == -1)
        {
            cerr << "epoll_wait error\n";
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
                    cerr << "Failed to accept client\n";
                    continue;
                }

                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) == -1)
                {
                    cerr << "Failed to add client socket to epoll\n";
                    tcp_server_.close_client(client_fd); // FIX
                    continue;
                }
                connection_manager_.add(client_fd);
                cout << "New client connected: fd " << client_fd << "\n";
            }
            else if (current_fd == timer_fd_)
            {
                uint64_t expirations;
                ssize_t n = ::read(timer_fd_, &expirations, sizeof(expirations));

                if (n == sizeof(expirations))
                {
                    if (!save_snapshot())
                        cerr << "Automatic snapshot failed\n";
                }
                else if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    cerr << "Failed to read snapshot timer\n";
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
        conn.append_data(msg);
        while (true)
        {
            auto [parse_status, input_resp, consumed] = RespParser::parse(conn.buffer());
            if (parse_status == ParseStatus::Incomplete)
                break;
            if (parse_status == ParseStatus::Invalid)
            {
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
            conn.consume(consumed);

            if (!tcp_server_.send_message(client_fd, output_msg))
            {
                cerr << "Failed to send message to client fd: " << client_fd << endl;
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
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        tcp_server_.close_client(client_fd);
        connection_manager_.remove(client_fd);
        return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return;
    }
    cerr << "Read error on client fd: " << client_fd << endl;
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
            cerr << "Corrupted AOF entry. Skipping restore.\n";
            return;
        }
        if (parse_status == ParseStatus::Incomplete)
        {
            cerr << "Truncated AOF entry detected. Restore aborted.\n";
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
        cerr << "error persisting cmd\n";
}

bool RedisServer::save_snapshot()
{
    cout << "starting save db snapshot\n";

    // sync aof to db now dump db snapshot to rdb then clear the aof file
    if (!aof_.fsync())
        return false;

    if (!rdb_.write(db_))
        return false;

    if (!aof_.truncate())
        return false;

    return true;
}

bool RedisServer::add_timer()
{
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd_ == -1)
    {
        cerr << "timerfd_create failed\n";
        return false;
    }

    itimerspec timer{};
    timer.it_value.tv_sec = 300;
    timer.it_interval.tv_sec = 300;

    if (timerfd_settime(timer_fd_, 0, &timer, nullptr) == -1)
    {
        cerr << "timerfd_settime failed\n";
        ::close(timer_fd_);
        timer_fd_ = -1;
        return false;
    }

    epoll_event ev{};
    ev.data.fd = timer_fd_;
    ev.events = EPOLLIN;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev) == -1)
    {
        cerr << "Failed to add timer to epoll\n";
        ::close(timer_fd_);
        timer_fd_ = -1;
        return false;
    }

    return true;
}