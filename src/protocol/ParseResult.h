#pragma once
#include <iostream>
#include "RespValue.h"
#include <optional>
using namespace std;

enum class ParseStatus
{
    Complete,
    Incomplete,
    Invalid
};

struct ParseResult
{
    ParseResult(ParseStatus s, optional<RespValue> v, size_t bytes) : status(s), value(v), bytes_consumed(bytes) {}
    ParseStatus status;
    optional<RespValue> value;
    size_t bytes_consumed;
};