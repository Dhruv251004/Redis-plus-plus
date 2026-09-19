#include <optional>
#include "command/command.h"
#include "RespValue.h"
#pragma once
using namespace std;

class RespParser
{
public:
    static RespValue ParseSimpleString(string_view);
    static RespValue ParseBulkString(string_view);
    static RespValue Parse(string_view);
};