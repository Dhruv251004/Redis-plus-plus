#pragma once
#include <sys/epoll.h>
#include <unordered_set>
#include "network/TcpServer.h"
#include "ConnectionManager.h"
#include "command/CommandExecutor.h"
#include "database/Database.h"
#include "persistence/AOF.h"
#include "persistence/RDB.h"

class RedisServer
{
public:
    RedisServer(int);
    void start();
    void stop();
    void handle_client(int);
    void handle_replay(string_view);
    static bool is_write_command(const string &);
    void persist_in_db(const RespValue &, const RespValue &);
    bool save_snapshot();
    bool add_timer();

private:
    static const unordered_set<string> write_command_;
    int epoll_fd_;
    int timer_fd_;
    TcpServer tcp_server_;
    ConnectionManager connection_manager_;
    Database db_;
    AOF aof_;
    CommandExecutor command_executor_;
    RDB rdb_;
};
