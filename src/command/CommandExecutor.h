#pragma once
#include <unordered_map>
#include <vector>
#include "protocol/RespValue.h"
#include "database/Database.h"
#include "persistence/AOF.h"
#include "pubsub/PubSubManager.h"

class CommandExecutor
{
public:
    CommandExecutor(Database &, PubSubManager &);
    RespValue execute(const RespValue &, int client_fd);

private:
    using Handler = RespValue (CommandExecutor::*)(const vector<RespValue> &, int);
    unordered_map<string, Handler> command_map_;
    Database &db_;
    PubSubManager &pub_sub_manager_;

    RespValue handle_ping(const vector<RespValue> &, int);
    RespValue handle_echo(const vector<RespValue> &, int);
    RespValue handle_set(const vector<RespValue> &, int);
    RespValue handle_get(const vector<RespValue> &, int);
    RespValue handle_del(const vector<RespValue> &, int);
    RespValue handle_exists(const vector<RespValue> &, int);
    RespValue handle_expire(const vector<RespValue> &, int);
    RespValue handle_ttl(const vector<RespValue> &, int);
    RespValue handle_persist(const vector<RespValue> &, int);
    RespValue handle_expire_at(const vector<RespValue> &, int);
    RespValue handle_pexpire_at(const vector<RespValue> &, int);
    RespValue handle_publish(const vector<RespValue> &, int);
    RespValue handle_subscribe(const vector<RespValue> &, int);
    RespValue handle_unsubscribe(const vector<RespValue> &, int);
    RespValue unknown_command(string_view);
    RespValue wrong_arg_count(string_view);
    RespValue syntax_error();
    RespValue invalid_integer();
};