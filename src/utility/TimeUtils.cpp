#include <iostream>
#include <chrono>
#include "TimeUtils.h"
using namespace std::chrono;

bool isExpired(int64_t expiry)
{
    if (expiry == NO_EXPIRY)
        return false;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return now >= expiry;
}

int64_t currentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}