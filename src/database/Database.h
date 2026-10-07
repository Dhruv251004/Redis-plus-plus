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
    struct KeyEntry
    {
        KeyEntry() {}
        KeyEntry(string data, int64_t expiry = NO_EXPIRY) : data(data), expiry(expiry) {}
        string data;
        int64_t expiry = NO_EXPIRY;
    };

    optional<string> get(const string &);
    optional<KeyEntry> get_key_entry(const string &);
    void set(const string &, const string &);
    void set(const string &, const string &, int64_t);
    bool expire(const string &, int64_t);
    bool exists(const string &);
    bool del(const string &);
    bool persist(const string &);
    optional<int64_t> ttl(const string &);
    size_t get_db_size() const;
    unordered_map<string, KeyEntry> get_all_data() const;

private:
    unordered_map<string, KeyEntry> in_memory_cache_;
};