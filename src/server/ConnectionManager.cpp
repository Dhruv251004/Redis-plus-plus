#include "ConnectionManager.h"

void ConnectionManager::add(int fd)
{
    connections_[fd] = Connection(fd);
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