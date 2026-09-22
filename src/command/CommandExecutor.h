#pragma once
#include "protocol/RespValue.h"
#include <unordered_map>
#include <vector>

class CommandExecutor
{
public:
    static RespValue execute(const RespValue &);

private:
    using Handler = RespValue (*)(const vector<RespValue> &);
    static const unordered_map<string, Handler> commandMap;

    static RespValue handle_ping(const vector<RespValue> &);
    static RespValue handle_echo(const vector<RespValue> &);
    // static RespValue handle_set(const vector<RespValue> &);

    static RespValue unknown_command(std::string_view);
    static RespValue wrong_arg_count(std::string_view);
};