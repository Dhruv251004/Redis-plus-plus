#pragma once
#include "server/ConnectionManager.h"
#include "protocol/RespValue.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

class PubSubManager
{
public:
    PubSubManager(ConnectionManager &);
    bool add_subscriber(std::string_view, int);
    bool remove_subscriber(std::string_view, int);
    bool remove_all_subscriptions(int);
    int publish(std::string_view, RespValue &);

private:
    ConnectionManager conn_mgr_;
    std::unordered_map<std::string, std::unordered_set<int>> subscribers_;
    std::unordered_map<int, std::unordered_set<std::string>> client_subs_list_;
};