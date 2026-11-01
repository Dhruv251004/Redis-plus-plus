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
    set(key, value, NO_EXPIRY);
}

void Database::set(const string &key, const string &value, int64_t expiry_time)
{
    auto it = in_memory_cache_.find(key);
    if (it != in_memory_cache_.end())
    {
        index_remove(key, it->second.expiry);
        it->second = KeyEntry(value, expiry_time);
    }
    else
    {
        in_memory_cache_.emplace(key, KeyEntry(value, expiry_time));
    }
    index_add(key, expiry_time);
}

bool Database::exists(const string &key)
{
    auto it = in_memory_cache_.find(key);
    if (it == in_memory_cache_.end())
        return false;
    if (is_expired(it->second.expiry))
    {
        index_remove(key, it->second.expiry);
        in_memory_cache_.erase(it);
        return false;
    }
    return true;
}
bool Database::del(const string &key)
{
    if (!exists(key))
        return false;
    auto it = in_memory_cache_.find(key);
    index_remove(key, it->second.expiry);
    in_memory_cache_.erase(it);
    return true;
}

bool Database::expire(const std::string &key, int64_t expiry_time)
{
    // exists() drops an already-expired key, so EXPIRE can't revive it
    if (!exists(key))
        return false;
    auto &entry = in_memory_cache_.at(key);
    index_remove(key, entry.expiry);
    entry.expiry = expiry_time;
    index_add(key, expiry_time);
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

    index_remove(key, entry->expiry);
    in_memory_cache_.at(key).expiry = NO_EXPIRY;
    return true;
}

size_t Database::purge_expired(size_t max_keys)
{
    size_t removed = 0;
    while (removed < max_keys && !expiry_index_.empty())
    {
        auto first = expiry_index_.begin();
        if (!is_expired(first->first))
            break;

        int64_t deadline = first->first;
        string key = first->second;
        expiry_index_.erase(first);

        // skip stale index entries whose key changed deadline
        auto it = in_memory_cache_.find(key);
        if (it != in_memory_cache_.end() && it->second.expiry == deadline)
        {
            in_memory_cache_.erase(it);
            removed++;
        }
    }
    return removed;
}

size_t Database::get_db_size() const
{
    return in_memory_cache_.size();
}

unordered_map<string, Database::KeyEntry> Database::get_all_data() const
{
    return in_memory_cache_;
}

void Database::index_add(const string &key, int64_t expiry)
{
    if (expiry != NO_EXPIRY)
        expiry_index_.emplace(expiry, key);
}

void Database::index_remove(const string &key, int64_t expiry)
{
    if (expiry != NO_EXPIRY)
        expiry_index_.erase({expiry, key});
}
