#pragma once
#include "network/Connection.h"
#include <unordered_map>
#include <functional>

using namespace std;
class ConnectionManager
{
public:
    void add(int);
    Connection &get(int);
    void remove(int);
    bool contains(int) const;
    size_t size() const;
    void notify_event(int, Connection &);
    void set_notify_handler(function<void(int, Connection &)>);

private:
    unordered_map<int, Connection> connections_;
    function<void(int, Connection &)> notify_handler_;
};