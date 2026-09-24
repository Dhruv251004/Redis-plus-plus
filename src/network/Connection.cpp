#include "Connection.h"

Connection::Connection(int socket_fd) : socket_fd_(socket_fd), receive_buffer_("") {}

int Connection::fd() const
{
    return socket_fd_;
}

void Connection::consume(size_t bytes)
{
    if (bytes >= receive_buffer_.size())
    {
        receive_buffer_ = "";
        return;
    }
    receive_buffer_.erase(0, bytes);
}

string_view Connection::buffer() const
{
    return receive_buffer_;
}

void Connection::append_data(string_view bytes)
{
    receive_buffer_ += bytes;
}