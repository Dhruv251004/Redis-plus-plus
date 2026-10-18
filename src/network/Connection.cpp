#include "Connection.h"

constexpr int COMPACT_THRESHOLD = 4096;

Connection::Connection(int socket_fd) : socket_fd_(socket_fd), read_buffer_(""), read_buffer_offset_(0), write_buffer_(""), write_buffer_offset_(0), close_after_write_(false), is_transaction_in_progress(false) {}

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

bool Connection::has_read_data() const
{
    return read_buffer_offset_ < int(read_buffer_.size());
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

bool Connection::has_write_data() const
{
    return write_buffer_offset_ < int(write_buffer_.size());
}

void Connection::set_close_after_write(bool val)
{
    close_after_write_ = val;
}

bool Connection::close_after_write() const
{
    return close_after_write_;
}

void Connection::set_transaction(bool val)
{
    is_transaction_in_progress = val;
}

bool Connection::is_under_transaction() const
{
    return is_transaction_in_progress;
}

void Connection::append_tx(const RespValue &cmd)
{
    tx_queue_.commands.push_back(cmd);
}
void Connection::set_tx_dirty(bool val)
{
    tx_queue_.dirty = val;
}
bool Connection::is_tx_dirty() const
{
    return tx_queue_.dirty;
}

void Connection::clear_tx_queue()
{
    tx_queue_.commands.clear();
    tx_queue_.dirty = false;
}

std::vector<RespValue> Connection::get_tx_queue() const
{
    return tx_queue_.commands;
}