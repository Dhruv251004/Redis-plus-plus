#include <optional>
#include "RespParser.h"
#include "ParserUtils.h"

using namespace std;

optional<Command> RespParser::parse(string_view buffer)
{
    int n = buffer.size();
    Command res;

    int ptr = 0;

    if (n == 0 || buffer[ptr] != '*')
        return nullopt;

    ptr++;

    auto arrLen = readNum(buffer, ptr);
    if (!arrLen || *arrLen == 0)
        return nullopt;

    if (!expect(buffer, ptr, "\r\n"))
        return nullopt;

    while (*arrLen > 0)
    {
        if (!expect(buffer, ptr, "$"))
            return nullopt;

        auto argLen = readNum(buffer, ptr);
        if (!argLen)
            return nullopt;

        if (!expect(buffer, ptr, "\r\n"))
            return nullopt;

        if (ptr + *argLen > n)
            return nullopt;

        string arg = readString(buffer, ptr, *argLen);

        // Consume the CRLF after the bulk string
        if (!expect(buffer, ptr, "\r\n"))
            return nullopt;

        if (res.name_.empty())
            res.name_ = arg;
        else
            res.args_.push_back(arg);

        (*arrLen)--;
    }

    return res;
}