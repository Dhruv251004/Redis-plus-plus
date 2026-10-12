#include "Connection.h"

constexpr int COMPACT_THRESHOLD = 4096;

Connection::Connection(int socket_fd) : socket_fd_(socket_fd), read_buffer_(""), read_buffer_offset_(0), write_buffer_(""), write_buffer_offset_(0) {}

int Connection::fd() const
{
    return socket_fd_;
}

void Connection::consume_read_buffer(size_t bytes)
{
    if (bytes + read_buffer_offset_ >= read_buffer_.size())
    {
        read_buffer_ = "";
        read_buffer_offset_ = 0;
        return;
    }

    read_buffer_offset_ += bytes;
    if (read_buffer_offset_ >= COMPACT_THRESHOLD && read_buffer_offset_ >= int(read_buffer_.size() / 2))
    {
        read_buffer_.erase(0, read_buffer_offset_);
        read_buffer_offset_ = 0;
    }
}

string_view Connection::read_buffer() const
{
    return string_view(read_buffer_).substr(read_buffer_offset_);
}

void Connection::append_read_data(string_view bytes)
{
    read_buffer_ += bytes;
}

void Connection::consume_write_buffer(size_t bytes)
{
    if (bytes + write_buffer_offset_ >= write_buffer_.size())
    {
        write_buffer_ = "";
        write_buffer_offset_ = 0;
        return;
    }

    write_buffer_offset_ += bytes;
    if (write_buffer_offset_ >= COMPACT_THRESHOLD && write_buffer_offset_ >= int(write_buffer_.size() / 2))
    {
        write_buffer_.erase(0, write_buffer_offset_);
        write_buffer_offset_ = 0;
    }
}

string_view Connection::write_buffer() const
{
    return string_view(write_buffer_).substr(write_buffer_offset_);
}

void Connection::append_write_data(string_view bytes)
{
    write_buffer_ += bytes;
}