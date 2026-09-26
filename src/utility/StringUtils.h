#pragma once

#include <string>
#include <cctype>

inline std::string uppercase(std::string_view input)
{
    std::string result(input);

    for (char &c : result)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    return result;
}