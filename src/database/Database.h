#pragma once
#include <iostream>
#include <unordered_map>
#include <optional>
using namespace std;

class Database
{
public:
    optional<string> get(const string &) const;
    void set(const string &, const string &);
    bool exists(const string &) const;
    void del(const string &);

private:
    unordered_map<string, string> in_memory_cache;
};