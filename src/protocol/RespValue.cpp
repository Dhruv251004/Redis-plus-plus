#include <iostream>
#include "RespValue.h"
using namespace std;

RespValue::RespValue(Type type, string_view data) : type_(type), data_(data) {}

RespValue RespValue::simple_string(string_view s)
{
    return RespValue(Type::SimpleString, s);
}

RespValue RespValue::bulk_string(string_view s)
{
    return RespValue(Type::BulkString, s);
}

RespValue RespValue::array(vector<RespValue> values)
{
    RespValue v(Type::Array, "");
    v.children_ = std::move(values);
    return v;
}

RespValue RespValue::null_bulk_string()
{
    RespValue v(Type::NullBulkString, "");
    return v;
}

RespValue RespValue::error(const string &msg)
{
    return RespValue(Type::Error, msg);
}

RespValue::Type RespValue::type() const
{
    return type_;
}

RespValue RespValue::integer(int num)
{
    return RespValue(Type::Integer, to_string(num));
}

const string &RespValue::data() const
{
    return data_;
}

const vector<RespValue> &RespValue::children() const
{
    return children_;
}

bool RespValue::is_error() const
{
    return type_ == Type::Error;
}