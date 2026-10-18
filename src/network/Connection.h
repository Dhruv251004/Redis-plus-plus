#pragma once
#include <iostream>
#include <vector>
#include "protocol/RespValue.h"

struct Transaction
{
    std::vector<RespValue> commands;
    bool dirty = false; // a queued command was invalid -> EXEC returns EXECABORT
};

class Connection
{
public:
    Connection(int);
    int fd() const;

    // receive buffer
    void consume_read_buffer(size_t);
    void append_read_data(std::string_view);
    std::string_view read_buffer() const;
    bool has_read_data() const;

    // send buffer
    void consume_write_buffer(size_t);
    void append_write_data(std::string_view);
    std::string_view write_buffer() const;
    bool has_write_data() const;

    void set_close_after_write(bool);
    bool close_after_write() const;

    void set_transaction(bool);
    bool is_under_transaction() const;

    void append_tx(const RespValue &);
    void set_tx_dirty(bool);
    bool is_tx_dirty() const;
    void clear_tx_queue();
    std::vector<RespValue> get_tx_queue() const;

private:
    int socket_fd_;
    Transaction tx_queue_;

    std::string read_buffer_;
    int read_buffer_offset_;

    std::string write_buffer_;
    int write_buffer_offset_;

    bool close_after_write_;
    bool is_transaction_in_progress;
};