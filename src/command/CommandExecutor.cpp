#include <iostream>
#include "protocol/RespValue.h"
#include "CommandExecutor.h"
#include "utility/StringUtils.h"
// #include
using namespace std;

CommandExecutor::CommandExecutor(Database &db) : db_(db)
{
    command_map_ = {
        {"PING", &CommandExecutor::handle_ping},
        {"ECHO", &CommandExecutor::handle_echo},
        {"GET", &CommandExecutor::handle_get},
        {"SET", &CommandExecutor::handle_set},
        {"DEL", &CommandExecutor::handle_del},
        {"EXISTS", &CommandExecutor::handle_exists},
    };
}

RespValue CommandExecutor::execute(const RespValue &resp)
{
    if (resp.type() != RespValue::Type::Array)
        return RespValue::error("ERR expected array");

    const auto &children = resp.children();
    if (children.size() == 0)
        return RespValue::error("ERR empty command");

    auto &cmd = children[0];
    if (cmd.type() != RespValue::Type::BulkString)
        return RespValue::error("ERR invalid command name");

    string cmdName = uppercase(cmd.data());
    auto it = command_map_.find(cmdName);
    if (it == command_map_.end())
        return unknown_command(cmdName);

    vector<RespValue> args(children.begin() + 1, children.end());
    return (this->*(it->second))(args);
}

RespValue CommandExecutor::handle_ping(const vector<RespValue> &args)
{
    if (!args.empty())
        return wrong_arg_count("PING");

    return RespValue::simple_string("PONG");
}

RespValue CommandExecutor::handle_echo(const vector<RespValue> &args)
{
    if (args.size() != 1)
        return wrong_arg_count("ECHO");
    return RespValue::bulk_string(args[0].data());
}

RespValue CommandExecutor::handle_set(const vector<RespValue> &args)
{
    if (args.size() != 2)
        return wrong_arg_count("SET");
    string key = args[0].data();
    string value = args[1].data();
    db_.set(key, value);

    return RespValue::simple_string("OK");
}

RespValue CommandExecutor::handle_get(const vector<RespValue> &args)
{
    if (args.size() != 1)
        return wrong_arg_count("GET");
    string key = args[0].data();
    auto value = db_.get(key);
    if (!value.has_value())
        return RespValue::null_bulk_string();
    return RespValue::bulk_string(*value);
}

RespValue CommandExecutor::handle_del(const vector<RespValue> &args)
{
    if (args.size() != 1)
        return wrong_arg_count("DEL");
    string key = args[0].data();

    if (!db_.exists(key))
        return RespValue::integer(0);

    db_.del(key);
    return RespValue::integer(1);
}

RespValue CommandExecutor::handle_exists(const vector<RespValue> &args)
{
    if (args.size() != 1)
        return wrong_arg_count("EXISTS");
    string key = args[0].data();

    if (!db_.exists(key))
        return RespValue::integer(0);

    return RespValue::integer(1);
}

RespValue CommandExecutor::unknown_command(string_view cmd)
{
    return RespValue::error("ERR unknown command '" + string(cmd) + "'");
}

RespValue CommandExecutor::wrong_arg_count(std::string_view cmd)
{
    return RespValue::error("ERR wrong number of arguments for '" + std::string(cmd) + "' command");
}