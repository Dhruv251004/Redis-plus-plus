#pragma once
#include <sys/epoll.h>
#include "network/TcpServer.h"
#include "ConnectionManager.h"

class RedisServer
{
public:
    RedisServer(int);
    void start();
    void stop();
    void handle_client(int);

private:
    int epoll_fd_;
    TcpServer tcp_server_;
    ConnectionManager connection_manager_;
};
