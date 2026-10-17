#include "PubSubManager.h"
#include "server/ConnectionManager.h"
#include "protocol/RespSerializer.h"

PubSubManager::PubSubManager(ConnectionManager &conn_mgr) : conn_mgr_(conn_mgr) {}

int PubSubManager::add_subscriber(string_view topic, int client_fd)
{
    string topic_str = string(topic);
    subscribers_[topic_str].insert(client_fd);

    client_subs_list_[client_fd].insert(topic_str);
    return static_cast<int>(subscribers_[topic_str].size());
}

int PubSubManager::remove_subscriber(string_view topic, int client_fd)
{
    string topic_str = string(topic);
    auto it = subscribers_.find(topic_str);
    if (it == subscribers_.end())
        return 0;

    // Update the client's own list first so it is cleaned up even when the topic becomes empty.
    auto client_it = client_subs_list_.find(client_fd);
    if (client_it != client_subs_list_.end())
    {
        client_it->second.erase(topic_str);
        if (client_it->second.empty())
            client_subs_list_.erase(client_it);
    }

    it->second.erase(client_fd);
    if (it->second.empty())
    {
        subscribers_.erase(it);
        return 0;
    }
    return static_cast<int>(it->second.size());
}

int PubSubManager::remove_all_subscriptions(int client_fd)
{

    auto &subs_list = client_subs_list_[client_fd];
    int len = subs_list.size();

    for (auto topic : subs_list)
    {
        subscribers_[topic].erase(client_fd);
        if (subscribers_[topic].empty())
        {
            subscribers_.erase(topic);
        }
    }
    client_subs_list_.erase(client_fd);
    return len;
}

int PubSubManager::subscription_count(int client_fd) const
{
    auto it = client_subs_list_.find(client_fd);
    return it == client_subs_list_.end() ? 0 : static_cast<int>(it->second.size());
}

int PubSubManager::publish(string_view topic, string_view serialized_msg, int publisher_fd)
{
    string topic_str = string(topic);
    auto it = subscribers_.find(topic_str);
    if (it == subscribers_.end())
        return 0;
    auto clients = it->second;

    for (auto subscribers_fd : clients)
    {
        Connection &conn = conn_mgr_.get(subscribers_fd);
        conn.append_write_data(serialized_msg);

        // The publisher is mid-way through handle_read_client; flushing it here could close and
        // erase its Connection under the caller. handle_client flushes it right after.
        if (subscribers_fd != publisher_fd)
            conn_mgr_.notify_event(subscribers_fd, conn);
    }
    return clients.size();
}