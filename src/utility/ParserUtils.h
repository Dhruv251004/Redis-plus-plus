
#include <iostream>
using namespace std;

bool expect(std::string_view buffer, int &ptr, std::string_view toRead)
{
    int len = buffer.size();
    int start = ptr;

    for (char c : toRead)
    {
        if (ptr >= len || buffer[ptr] != c)
        {
            ptr = start; // rollback on failure
            return false;
        }
        ptr++;
    }

    return true;
}

optional<int> readNum(string_view buffer, int &ptr)
{
    int len = buffer.size();
    if (ptr >= len || !isdigit(static_cast<unsigned char>(buffer[ptr])))
        return nullopt;

    int num = 0;

    while (ptr < len && isdigit(static_cast<unsigned char>(buffer[ptr])))
    {
        num = num * 10 + (buffer[ptr] - '0');
        ptr++;
    }

    return num;
}

string readString(string_view buffer, int &ptr, int len)
{
    string res;
    while (len--)
        res += buffer[ptr++];
    return res;
}