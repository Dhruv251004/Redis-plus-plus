#pragma once
#include "network/Connection.h"
#include <unordered_map>

using namespace std;
class ConnectionManager
{
public:
    void add(int);
    Connection &get(int);
    void remove(int);
    bool contains(int) const;
    size_t size() const;

private:
    unordered_map<int, Connection> connections_;
};