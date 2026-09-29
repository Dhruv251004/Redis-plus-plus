#include <iostream>
#include "protocol/RespValue.h"
#include "CommandExecutor.h"
#include "utility/StringUtils.h"
#include "persistence/AOF.h"

using namespace std;

CommandExecutor::CommandExecutor(Database &db, AOF &aof) : db_(db), aof_(aof)
{
    command_map_ = {
        {"PING", &CommandExecutor::handle_ping},
        {"ECHO", &CommandExecutor::handle_echo},
        {"GET", &CommandExecutor::handle_get},
        {"SET", &CommandExecutor::handle_set},
        {"DEL", &CommandExecutor::handle_del},
        {"EXISTS", &CommandExecutor::handle_exists},
        {"EXPIRE", &CommandExecutor::handle_expire},
        {"TTL", &CommandExecutor::handle_ttl},
        {"PERSIST", &CommandExecutor::handle_persist},
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

    return (this->*(it->second))(resp);
}

RespValue CommandExecutor::handle_ping(const RespValue &command)
{
    const auto &args = command.children();
    if (!args.empty())
        return wrong_arg_count("PING");

    return RespValue::simple_string("PONG");
}

RespValue CommandExecutor::handle_echo(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("ECHO");
    return RespValue::bulk_string(args[0].data());
}

RespValue CommandExecutor::handle_set(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 2 && args.size() != 4)
        return wrong_arg_count("SET");

    string key = args[0].data();
    string value = args[1].data();

    if (args.size() > 2)
    {
        string cmd = args[2].data();
        if (cmd != "EX")
            return syntax_error();
        auto expiry = parse_integer(args[3].data());
        if (!expiry.has_value())
            return invalid_integer();
        db_.set(key, value, *expiry);
    }
    else
    {
        db_.set(key, value);
    }

    return RespValue::simple_string("OK");
}

RespValue CommandExecutor::handle_get(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("GET");
    string key = args[0].data();
    auto value = db_.get(key);
    if (!value.has_value())
        return RespValue::null_bulk_string();
    return RespValue::bulk_string(*value);
}

RespValue CommandExecutor::handle_del(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("DEL");
    string key = args[0].data();

    return RespValue::integer(db_.del(key) ? 1 : 0);
}

RespValue CommandExecutor::handle_exists(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("EXISTS");
    string key = args[0].data();

    return RespValue::integer(db_.exists(key) ? 1 : 0);
}

RespValue CommandExecutor::handle_expire(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 2)
        return wrong_arg_count("EXPIRE");

    string key = args[0].data();
    string ttl = args[1].data();

    auto seconds = parse_integer(args[1].data());
    if (!seconds.has_value())
        return invalid_integer();

    return RespValue::integer(db_.expire(key, *seconds) ? 1 : 0);
}

RespValue CommandExecutor::handle_ttl(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("TTL");

    auto expiry = db_.ttl(args[0].data());
    if (!expiry.has_value())
        return RespValue::integer(-2);
    if (*expiry == NO_EXPIRY)
        return RespValue::integer(-1);

    int64_t remaining = (*expiry - currentTimeMs()) / 1000;
    return RespValue::integer(remaining);
}

RespValue CommandExecutor::handle_persist(const RespValue &command)
{
    const auto &args = command.children();
    if (args.size() != 1)
        return wrong_arg_count("PERSIST");
    return RespValue::integer(db_.persist(args[0].data()) ? 1 : 0);
}

RespValue CommandExecutor::unknown_command(string_view cmd)
{
    return RespValue::error("ERR unknown command '" + string(cmd) + "'");
}

RespValue CommandExecutor::wrong_arg_count(string_view cmd)
{
    return RespValue::error("ERR wrong number of arguments for '" + string(cmd) + "' command");
}

RespValue CommandExecutor::invalid_integer()
{
    return RespValue::error("ERR value is not an integer or out of range");
}

RespValue CommandExecutor::syntax_error()
{
    return RespValue::error("ERR syntax error");
}