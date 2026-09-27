#pragma once
#include <unordered_map>
#include <vector>
#include "protocol/RespValue.h"
#include "database/Database.h"
using namespace std;

class CommandExecutor
{
public:
    CommandExecutor(Database &);
    RespValue execute(const RespValue &);

private:
    using Handler = RespValue (CommandExecutor::*)(const vector<RespValue> &);
    unordered_map<string, Handler> command_map_;
    Database &db_;

    RespValue handle_ping(const vector<RespValue> &);
    RespValue handle_echo(const vector<RespValue> &);
    RespValue handle_set(const vector<RespValue> &);
    RespValue handle_get(const vector<RespValue> &);
    RespValue handle_del(const vector<RespValue> &);
    RespValue handle_exists(const vector<RespValue> &);
    RespValue handle_expire(const vector<RespValue> &);
    RespValue handle_ttl(const vector<RespValue> &);
    RespValue handle_persist(const vector<RespValue> &);
    RespValue unknown_command(string_view);
    RespValue wrong_arg_count(string_view);
    RespValue invalid_integer();
};