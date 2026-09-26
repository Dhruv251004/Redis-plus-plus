#include <cerrno>
#include <iostream>
#include <sys/epoll.h>
#include "RedisServer.h"
#include "protocol/RespParser.h"
#include "command/CommandExecutor.h"
#include "protocol/RespSerializer.h"
#include "protocol/ParseResult.h"

using namespace std;

constexpr int MAX_EVENTS = 64;

RedisServer::RedisServer(int port) : tcp_server_(port) {}

void RedisServer::start()
{
    bool res = tcp_server_.start();
    if (!res)
        return;

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

            conn.consume(consumed);
            RespValue output_resp = CommandExecutor::execute(*input_resp);
            string output_msg = RespSerializer::serialize(output_resp);

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

void RedisServer::stop()
{
    tcp_server_.stop();
}