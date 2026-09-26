
#include <iostream>
#include "protocol/ParseResult.h"
using namespace std;

ParseStatus expect(std::string_view buffer, int &ptr, std::string_view toRead)
{
    int len = buffer.size();
    int start = ptr;

    for (char c : toRead)
    {
        if (ptr >= len)
            return ParseStatus::Incomplete;
        if (buffer[ptr] != c)
        {
            return ParseStatus::Invalid;
        }
        ptr++;
    }

    return ParseStatus::Complete;
}

pair<ParseStatus, int> readNum(string_view buffer, int &ptr)
{
    int len = buffer.size();
    if (ptr >= len)
        return {ParseStatus::Incomplete, 0};
    if (!isdigit(static_cast<unsigned char>(buffer[ptr])))
        return {ParseStatus::Invalid, 0};

    int num = 0;
    while (ptr < len && isdigit(static_cast<unsigned char>(buffer[ptr])))
    {
        num = num * 10 + (buffer[ptr] - '0');
        ptr++;
    }

    return {ParseStatus::Complete, num};
}

string readString(string_view buffer, int &ptr, int len)
{
    string res;
    while (len--)
        res += buffer[ptr++];
    return res;
}