#include <iostream>
#include "Database.h"
#include "utility/TimeUtils.h"
using namespace std;

optional<string> Database::get(const string &key)
{
    if (!exists(key))
        return nullopt;
    return in_memory_cache_.at(key).data;
}

optional<Database::KeyEntry> Database::get_key_entry(const string &key)
{
    if (!exists(key))
        return nullopt;
    return in_memory_cache_.at(key);
}

void Database::set(const string &key, const string &value)
{
    in_memory_cache_[key] = KeyEntry(value);
}

bool Database::exists(const string &key)
{
    auto it = in_memory_cache_.find(key);
    if (it == in_memory_cache_.end())
        return false;
    if (isExpired(it->second.expiry))
    {
        in_memory_cache_.erase(it);
        return false;
    }
    return true;
}
bool Database::del(const string &key)
{
    if (!exists(key))
        return false;
    in_memory_cache_.erase(key);
    return true;
}

bool Database::expire(const std::string &key, int64_t seconds)
{
    auto it = in_memory_cache_.find(key);
    if (it == in_memory_cache_.end())
        return false;
    it->second.expiry = currentTimeMs() + seconds * 1000;
    return true;
}
optional<int64_t> Database::ttl(const string &key)
{
    auto entry = get_key_entry(key);
    if (!entry.has_value())
        return nullopt;
    return entry->expiry;
}

bool Database::persist(const string &key)
{
    auto entry = get_key_entry(key);
    if (!entry.has_value() || entry->expiry == NO_EXPIRY)
        return false;

    in_memory_cache_.at(key).expiry = NO_EXPIRY;
    return true;
}