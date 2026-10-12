#pragma once
#include <iostream>
using namespace std;

class Connection
{
public:
    Connection(int);
    int fd() const;

    // receive buffer
    void consume_read_buffer(size_t);
    void append_read_data(string_view);
    string_view read_buffer() const;

    // send buffer
    void consume_write_buffer(size_t);
    void append_write_data(string_view);
    string_view write_buffer() const;

private:
    int socket_fd_;

    string read_buffer_;
    int read_buffer_offset_;

    string write_buffer_;
    int write_buffer_offset_;
};