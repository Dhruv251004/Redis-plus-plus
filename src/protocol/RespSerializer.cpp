#include "RespSerializer.h"

string RespSerializer::serialize(const RespValue &res)
{
    string serialized_res;
    if (res.type() == RespValue::Type::SimpleString)
        serialized_res = serialize_simple_string(res.data());
    else if (res.type() == RespValue::Type::BulkString)
        serialized_res = serialize_bulk_string(res.data());
    else if (res.type() == RespValue::Type::Array)
    {
        const vector<RespValue> &children = res.children();
        serialized_res = "*" + to_string(children.size());
        serialized_res += "\r\n";
        for (const auto &child : children)
            serialized_res += serialize(child);
    }
    else if (res.type() == RespValue::Type::Integer)
        serialized_res = serialize_integer(res.data());
    else if (res.type() == RespValue::Type::Error)
        serialized_res = serialize_error(res.data());
    else if (res.type() == RespValue::Type::NullBulkString)
        serialized_res = serialize_null_bulk_string();
    else
        serialized_res = serialize_error("ERR unsupported response type");

    return serialized_res;
}

string RespSerializer::serialize_simple_string(string_view data)
{
    string res = "+";
    res += data;
    res += "\r\n";
    return res;
}

string RespSerializer::serialize_bulk_string(string_view data)
{
    string res = "$";
    res += to_string(data.size());
    res += "\r\n";
    res += data;
    res += "\r\n";
    return res;
}

string RespSerializer::serialize_error(string_view data)
{
    string res = "-";
    res += data;
    res += "\r\n";
    return res;
}

string RespSerializer::serialize_null_bulk_string()
{
    return "$-1\r\n";
}

string RespSerializer::serialize_integer(string_view data)
{
    return ":" + string(data) + "\r\n";
}