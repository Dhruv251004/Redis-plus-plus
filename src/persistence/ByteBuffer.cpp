#include "ByteBuffer.h"
#include <iostream>
#include <cmath>
#include <cstdint>
#include <optional>
using namespace std;

ByteBuffer::ByteBuffer() : offset_(0) {}

ByteBuffer::ByteBuffer(vector<uint8_t> &&buffer) : buffer_(move(buffer)), offset_(0) {};

void ByteBuffer::write_string(string_view str)
{
    for (auto ch : str)
    {
        int byte = static_cast<uint8_t>(ch);
        buffer_.push_back(byte);
    }
}

void ByteBuffer::write_u8(uint8_t num)
{
    buffer_.push_back(num);
}

void ByteBuffer::write_u16(uint16_t num)
{
    uint8_t mask = 255;
    for (int i = 1; i >= 0; i--)
    {
        uint8_t tmp_mask = mask << (i * 8);
        uint8_t byte = (num & tmp_mask) >> (i * 8);
        buffer_.push_back(byte);
    }
}

void ByteBuffer::write_u32(uint32_t num)
{
    uint8_t mask = 255;
    for (int i = 3; i >= 0; i--)
    {
        uint8_t tmp_mask = mask << (i * 8);
        uint8_t byte = (num & tmp_mask) >> (i * 8);
        buffer_.push_back(byte);
    }
}

void ByteBuffer::write_u64(uint64_t num)
{
    uint8_t mask = 255;
    for (int i = 7; i >= 0; i--)
    {
        uint8_t tmp_mask = mask << (i * 8);
        uint8_t byte = (num & tmp_mask) >> (i * 8);
        buffer_.push_back(byte);
    }
}

optional<string> ByteBuffer::read_string(uint32_t len)
{
    if (len == 0)
        return "";
    if (offset_ + len > buffer_.size())
        return nullopt;

    string res;
    while (len--)
    {
        res.push_back(static_cast<char>(buffer_[offset_++]));
    }

    return res;
}

optional<uint8_t> ByteBuffer::read_u8()
{
    int64_t bytes_to_read = 1;
    if (offset_ + bytes_to_read > buffer_.size())
        return nullopt;

    uint8_t num = 0;
    while (bytes_to_read--)
    {
        num = num * 256 + buffer_[offset_++];
    }
    return num;
}

optional<uint16_t> ByteBuffer::read_u16()
{
    int64_t bytes_to_read = 2;
    if (offset_ + bytes_to_read > buffer_.size())
        return nullopt;

    uint16_t num = 0;
    while (bytes_to_read--)
    {
        num = num * 256 + buffer_[offset_++];
    }
    return num;
}
optional<uint32_t> ByteBuffer::read_u32()
{
    int64_t bytes_to_read = 4;
    if (offset_ + bytes_to_read > buffer_.size())
        return nullopt;

    uint32_t num = 0;
    while (bytes_to_read--)
    {
        num = num * 256 + buffer_[offset_++];
    }
    return num;
}
optional<uint64_t> ByteBuffer::read_u64()
{
    int64_t bytes_to_read = 8;
    if (offset_ + bytes_to_read > buffer_.size())
        return nullopt;

    uint64_t num = 0;
    while (bytes_to_read--)
    {
        num = num * 256 + buffer_[offset_++];
    }
    return num;
}

const uint8_t *ByteBuffer::data() const
{
    if (buffer_.size() == 0)
        return nullptr;
    return &buffer_[0];
}

size_t ByteBuffer::size() const
{
    return buffer_.size();
}

uint32_t ByteBuffer::crc32(size_t start, size_t end) const
{
    if (buffer_.empty() || start > end || end >= buffer_.size())
        return ::crc32(0L, Z_NULL, 0);
    size_t len = end - start + 1;
    return ::crc32(0LL, &buffer_[start], len);
}

void ByteBuffer::write_bytes(const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        buffer_.push_back(bytes[i]);
    }
}
