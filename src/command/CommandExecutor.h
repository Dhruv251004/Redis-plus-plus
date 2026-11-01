#pragma once
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include "protocol/RespValue.h"
#include "database/Database.h"
#include "persistence/AOF.h"
#include "pubsub/PubSubManager.h"
#include "network/Connection.h"

class CommandExecutor
{
public:
    // Fired after a write changes the db. Queued or failed commands never fire it.
    // An EXEC fires MULTI, each write, then EXEC (nothing if no write changed anything).
    using WriteObserver = std::function<void(const RespValue &command, const RespValue &result)>;

    CommandExecutor(Database &, PubSubManager &);
    RespValue execute(const RespValue &, Connection &);
    void set_write_observer(WriteObserver);

private:
    using Handler = RespValue (CommandExecutor::*)(const vector<RespValue> &, int);
    unordered_map<string, Handler> command_map_;
    Database &db_;
    PubSubManager &pub_sub_manager_;
    WriteObserver write_observer_;
    bool in_exec_ = false;
    bool exec_logged_ = false; // MULTI already sent to the observer for this EXEC

    static const unordered_set<string> write_commands_;
    static bool changed_data(const string &, const RespValue &);
    void notify_write(const RespValue &, const RespValue &);

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
    optional<RespValue> handle_transaction(const string &, const RespValue &, Connection &, bool);
    RespValue unknown_command(string_view);
    RespValue wrong_arg_count(string_view);
    RespValue syntax_error();
    RespValue invalid_integer();
};