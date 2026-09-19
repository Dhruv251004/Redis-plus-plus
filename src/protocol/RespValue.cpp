#include <iostream>
#include "RespValue.h"
using namespace std;

RespValue::RespValue(Type type, string_view data) : type_(type), data_(data) {}

RespValue RespValue::SimpleString(string_view s)
{
    return RespValue(Type::SimpleString, s);
}

RespValue RespValue::BulkString(string_view s)
{
    return RespValue(Type::BulkString, s);
}

RespValue RespValue::Array(vector<RespValue> values)
{
    RespValue v(Type::Array, "");
    v.children_ = std::move(values);
    return v;
}

RespValue RespValue::Error(const string &msg)
{
    return RespValue(Type::Error, msg);
}

RespValue::Type RespValue::type() const
{
    return type_;
}

const string &RespValue::data() const
{
    return data_;
}

const vector<RespValue> &RespValue::Children() const
{
    return children_;
}

bool RespValue::isError() const
{
    return type_ == Type::Error;
}
