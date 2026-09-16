#include <optional>
#include "command/command.h"
#pragma once
using namespace std;

class RespParser
{
public:
    RespParser();
    optional<Command> parse(std::string_view);
};