#include "network/TcpServer.h"
#include "ConnectionManager.h"
#pragma once

class RedisServer
{
public:
    RedisServer(int);
    void start();
    void stop();
    void handle_client(int);

private:
    TcpServer tcp_server_;
    ConnectionManager connection_manager_;
};
