#include "RespValue.h"
#include "RespParser.h"
#include "utility/ParserUtils.h"
using namespace std;

ParseResult RespParser::parse(string_view buffer)
{
    int ptr = 0;

    if (buffer.empty())
        return ParseResult(ParseStatus::Incomplete, nullopt, 0);

    if (buffer[ptr] != '*')
        return ParseResult(ParseStatus::Invalid, RespValue::error("ERR expected array"), ptr);

    ptr++;

    auto [status, arrLen] = readNum(buffer, ptr);

    if (status == ParseStatus::Invalid || arrLen < 0)
        return ParseResult(ParseStatus::Invalid, RespValue::error("ERR invalid array length"), ptr);

    if (status == ParseStatus::Incomplete)
        return ParseResult(ParseStatus::Incomplete, nullopt, 0);

    status = expect(buffer, ptr, "\r\n");

    if (status == ParseStatus::Invalid)
        return ParseResult(ParseStatus::Invalid, RespValue::error("ERR expected CRLF after array length"), ptr);

    if (status == ParseStatus::Incomplete)
        return ParseResult(ParseStatus::Incomplete, nullopt, 0);

    vector<RespValue> res;
    res.reserve(arrLen);

    while (arrLen--)
    {
        ParseResult value = parse_bulk_string(buffer, ptr);

        if (value.status == ParseStatus::Invalid)
            return value;

        if (value.status == ParseStatus::Incomplete)
            return value;

        res.push_back(*move(value.value));
    }

    return ParseResult(ParseStatus::Complete, RespValue::array(move(res)), ptr);
}

ParseResult RespParser::parse_bulk_string(string_view s, int &ptr)
{
    ParseStatus status = expect(s, ptr, "$");
    if (status == ParseStatus::Invalid)
        return ParseResult(status, RespValue::error("ERR expected bulk string"), ptr);
    if (status == ParseStatus::Incomplete)
        return ParseResult(status, nullopt, 0);

    auto [numStatus, num] = readNum(s, ptr);
    if (numStatus == ParseStatus::Invalid)
        return ParseResult(numStatus, RespValue::error("ERR invalid bulk string length"), ptr);
    if (numStatus == ParseStatus::Incomplete)
        return ParseResult(numStatus, nullopt, 0);

    status = expect(s, ptr, "\r\n");
    if (status == ParseStatus::Invalid)
        return ParseResult(status, RespValue::error("ERR expected CRLF after bulk string length"), ptr);
    if (status == ParseStatus::Incomplete)
        return ParseResult(status, nullopt, 0);

    if (ptr + num > s.size())
        return ParseResult(ParseStatus::Incomplete, nullopt, 0);

    string data = readString(s, ptr, num);

    status = expect(s, ptr, "\r\n");
    if (status == ParseStatus::Invalid)
        return ParseResult(status, RespValue::error("ERR expected CRLF after bulk string"), ptr);
    if (status == ParseStatus::Incomplete)
        return ParseResult(status, nullopt, 0);

    return ParseResult(ParseStatus::Complete, RespValue::bulk_string(data), ptr);
}

ParseResult RespParser::parse_simple_string(std::string_view s)
{
    int ptr = 0;

    ParseStatus status = expect(s, ptr, "+");

    if (status == ParseStatus::Invalid)
        return ParseResult(status, RespValue::error("ERR expected simple string"), ptr);

    if (status == ParseStatus::Incomplete)
        return ParseResult(status, nullopt, 0);

    size_t end = s.find("\r\n", ptr);

    if (end == std::string_view::npos)
        return ParseResult(ParseStatus::Incomplete, nullopt, 0);

    int dataLen = static_cast<int>(end) - ptr;
    std::string data = readString(s, ptr, dataLen);

    status = expect(s, ptr, "\r\n");

    if (status == ParseStatus::Invalid)
        return ParseResult(status, RespValue::error("ERR expected CRLF after simple string"), ptr);

    if (status == ParseStatus::Incomplete)
        return ParseResult(status, nullopt, 0);

    return ParseResult(status, RespValue::simple_string(data), ptr);
}