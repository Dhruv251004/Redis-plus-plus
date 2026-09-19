#include <iostream>
#include "RespValue.h"
#include "RespParser.h"
#include "ParserUtils.h"
using namespace std;

RespValue RespParser::parse(string_view buffer)
{
    int n = buffer.size();

    int ptr = 0;

    if (n == 0 || buffer[ptr] != '*')
        return RespValue::error("ERR expected array");

    ptr++;

    auto arrLen = readNum(buffer, ptr);
    if (!arrLen || *arrLen < 0)
        return RespValue::error("ERR invalid array length");

    if (!expect(buffer, ptr, "\r\n"))
        return RespValue::error("ERR expected CRLF after array length");

    vector<RespValue> res;
    while (*arrLen > 0)
    {
        if (!expect(buffer, ptr, "$"))
            return RespValue::error("ERR expected bulk string");

        auto argLen = readNum(buffer, ptr);
        if (!argLen)
            return RespValue::error("ERR invalid bulk string length");

        if (!expect(buffer, ptr, "\r\n"))
            return RespValue::error("ERR expected CRLF after bulk string length");

        if (ptr + *argLen > n)
            return RespValue::error("ERR incomplete bulk string");

        string arg = readString(buffer, ptr, *argLen);

        if (!expect(buffer, ptr, "\r\n"))
            return RespValue::error("ERR expected CRLF after bulk string");

        res.push_back(RespValue::bulk_string(arg));
        (*arrLen)--;
    }

    return RespValue::array(move(res));
}

RespValue RespParser::parse_bulk_string(string_view s)
{
    int len = s.size();
    int ptr = 0;

    if (!expect(s, ptr, "$"))
        return RespValue::error("ERR expected bulk string");

    optional<int> num = readNum(s, ptr);
    if (!num.has_value())
        return RespValue::error("ERR invalid bulk string length");

    if (!expect(s, ptr, "\r\n"))
        return RespValue::error("ERR expected CRLF after bulk string length");

    string data = readString(s, ptr, *num);
    if (!expect(s, ptr, "\r\n"))
        return RespValue::error("ERR expected CRLF after bulk string");

    return RespValue::bulk_string(data);
}

RespValue RespParser::parse_simple_string(string_view s)
{
    int len = s.size();
    int ptr = 0;

    if (len < 3 || !expect(s, ptr, "+"))
        return RespValue::error("ERR expected simple string");

    int dataEnd = len - 3;
    int dataLen = dataEnd - ptr + 1;
    string data = readString(s, ptr, dataLen);

    if (!expect(s, ptr, "\r\n"))
        return RespValue::error("ERR expected CRLF after simple string");

    return RespValue::simple_string(data);
}