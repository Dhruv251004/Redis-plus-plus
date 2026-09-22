#pragma once
#include <string_view>
#include "RespValue.h"
using namespace std;

class RespParser
{
public:
    static RespValue parse_simple_string(string_view);
    static RespValue parse_bulk_string(string_view);
    static RespValue parse(string_view);
};