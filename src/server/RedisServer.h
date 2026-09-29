#pragma once
#include <sys/epoll.h>
#include "network/TcpServer.h"
#include "ConnectionManager.h"
#include "command/CommandExecutor.h"
#include "database/Database.h"
#include "persistence/AOF.h"

class RedisServer
{
public:
    RedisServer(int);
    void start();
    void stop();
    void handle_client(int);
    void handle_replay(string_view);

private:
    int epoll_fd_;
    TcpServer tcp_server_;
    ConnectionManager connection_manager_;
    Database db_;
    AOF aof_;
    CommandExecutor command_executor_;
};
