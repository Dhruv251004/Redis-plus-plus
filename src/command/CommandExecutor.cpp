#include <iostream>
#include "protocol/RespValue.h"
#include "CommandExecutor.h"
#include "utility/stringUtils.h"
// #include
using namespace std;

const unordered_map<string, CommandExecutor::Handler> CommandExecutor::commandMap = {
    {"PING", CommandExecutor::handle_ping},
    {"ECHO", CommandExecutor::handle_echo},
    // {"SET", CommandExecutor::handle_set},
};

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
    auto it = commandMap.find(cmdName);
    if (it == commandMap.end())
        return unknown_command(cmdName);

    vector<RespValue> args(children.begin() + 1, children.end());
    return it->second(args);
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

// RespValue CommandExecutor::handle_set(const vector<RespValue> &args)
// {
//     // some processing
// }

RespValue CommandExecutor::unknown_command(string_view cmd)
{
    return RespValue::error("ERR unknown command '" + string(cmd) + "'");
}

RespValue CommandExecutor::wrong_arg_count(std::string_view cmd)
{
    return RespValue::error("ERR wrong number of arguments for '" + std::string(cmd) + "' command");
}