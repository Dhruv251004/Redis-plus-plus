#pragma once
#include <iostream>
using namespace std;

class Connection
{
public:
    Connection(int);
    int fd() const;
    void consume(size_t);
    void append_data(string_view);
    string_view buffer() const;

private:
    int socket_fd_;
    string receive_buffer_;
};