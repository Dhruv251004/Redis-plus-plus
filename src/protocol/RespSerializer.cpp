#include "RespSerializer.h"

string RespSerializer::serialize(const RespValue &res)
{
    string serialzied_res;
    if (res.type() == RespValue::Type::SimpleString)
        serialzied_res = "+" + res.data() + "\r\n";
    else if (res.type() == RespValue::Type::BulkString)
        serialzied_res = "$" + to_string(res.data().size()) + "\r\n" + res.data() + "\r\n";
    else if (res.type() == RespValue::Type::Array)
    {
        const vector<RespValue> &children = res.children();
        serialzied_res = "*" + to_string(children.size());
        serialzied_res += "\r\n";
        for (const auto &child : children)
            serialzied_res += serialize(child);
    }
    else if (res.type() == RespValue::Type::Error)
        serialzied_res = "-" + res.data() + "\r\n";

    return serialzied_res;
}