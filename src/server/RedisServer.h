#include "network/TcpServer.h"
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
};
