#pragma once
#include <iostream>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include "utility/TimeUtils.h"
using namespace std;

class Database
{

public:
    optional<string> get(const string &);
    void set(const string &, const string &);
    bool expire(const string &, int64_t);
    bool exists(const string &);
    bool del(const string &);

private:
    struct KeyEntry
    {
        KeyEntry() {}
        KeyEntry(string data, int64_t expiry = NO_EXPIRY) : data(data), expiry(expiry) {}
        string data;
        int64_t expiry = NO_EXPIRY;
    };
    unordered_map<string, KeyEntry> in_memory_cache_;
};