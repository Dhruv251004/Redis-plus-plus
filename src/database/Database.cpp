#include "Database.h"
#include <iostream>
using namespace std;

optional<string> Database::get(const string &key) const
{
    if (!contains(key))
        return nullopt;
    return in_memory_cache.at(key);
}

void Database::set(const string &key, const string &value)
{
    in_memory_cache[key] = value;
}

bool Database::contains(const string &key) const
{
    return in_memory_cache.find(key) != in_memory_cache.end();
}
void Database::del(const string &key)
{
    if (!contains(key))
        return;
    in_memory_cache.erase(key);
}