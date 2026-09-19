#include <optional>
#include "command/command.h"
#include "RespValue.h"
#pragma once
using namespace std;

class RespParser
{
public:
    static RespValue parse_simple_string(string_view);
    static RespValue parse_bulk_string(string_view);
    static RespValue parse(string_view);
};