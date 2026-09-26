#pragma once
#include <string_view>
#include "ParseResult.h"
using namespace std;

class RespParser
{
public:
    static ParseResult parse_simple_string(string_view);
    static ParseResult parse_bulk_string(string_view, int &);
    static ParseResult parse(string_view);
};