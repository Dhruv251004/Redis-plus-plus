#include <iostream>
#include <chrono>
#include "TimeUtils.h"
using namespace std::chrono;

bool is_expired(int64_t expiry)
{
    if (expiry == NO_EXPIRY)
        return false;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return now >= expiry;
}

int64_t current_time_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t absolute_time_ms(int64_t seconds)
{
    int64_t now = current_time_ms();
    return (seconds > (INT64_MAX - now) / 1000) ? INT64_MAX : now + seconds * 1000;
}

int64_t seconds_to_ms(int64_t seconds)
{
    if (seconds > INT64_MAX / 1000)
        return INT64_MAX;
    if (seconds < INT64_MIN / 1000)
        return INT64_MIN;
    return seconds * 1000;
}
