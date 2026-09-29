#pragma once
#include <unordered_map>
#include <vector>
#include "protocol/RespValue.h"
#include "database/Database.h"
#include "persistence/AOF.h"

using namespace std;

class CommandExecutor
{
public:
    CommandExecutor(Database &, AOF &);
    RespValue execute(const RespValue &);

private:
    using Handler = RespValue (CommandExecutor::*)(const RespValue &);
    unordered_map<string, Handler> command_map_;
    Database &db_;
    AOF &aof_;

    RespValue handle_ping(const RespValue &);
    RespValue handle_echo(const RespValue &);
    RespValue handle_set(const RespValue &);
    RespValue handle_get(const RespValue &);
    RespValue handle_del(const RespValue &);
    RespValue handle_exists(const RespValue &);
    RespValue handle_expire(const RespValue &);
    RespValue handle_ttl(const RespValue &);
    RespValue handle_persist(const RespValue &);
    RespValue unknown_command(string_view);
    RespValue wrong_arg_count(string_view);
    RespValue syntax_error();
    RespValue invalid_integer();
};