#include "ConnectionManager.h"

void ConnectionManager::add(int fd)
{
    connections_.try_emplace(fd, fd);
}

void ConnectionManager::remove(int fd)
{
    connections_.erase(fd);
}

Connection &ConnectionManager::get(int fd)
{
    return connections_.at(fd);
}

bool ConnectionManager::contains(int fd) const
{
    return connections_.find(fd) != connections_.end();
}

size_t ConnectionManager::size() const
{
    return connections_.size();
}

void ConnectionManager::notify_event(int client_fd, Connection &conn)
{
    notify_handler_(client_fd, conn);
}

void ConnectionManager::set_notify_handler(function<void(int, Connection &)> notify_handler)
{
    notify_handler_ = std::move(notify_handler);
}