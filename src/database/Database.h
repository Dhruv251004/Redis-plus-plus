#pragma once
#include <iostream>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <set>
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
    // Removes up to max_keys expired keys; returns how many.
    size_t purge_expired(size_t max_keys = SIZE_MAX);
    size_t get_db_size() const;
    unordered_map<string, KeyEntry> get_all_data() const;

private:
    void index_add(const string &, int64_t);
    void index_remove(const string &, int64_t);

    unordered_map<string, KeyEntry> in_memory_cache_;
    // (expiry, key) of keys with an expiry, soonest first. Keep in sync with in_memory_cache_.
    std::set<std::pair<int64_t, std::string>> expiry_index_;
};