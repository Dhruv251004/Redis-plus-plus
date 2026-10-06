#pragma once
#include <iostream>
#include <vector>
#include <cstdint>
#include <optional>

class ByteBuffer
{
public:
    ByteBuffer();
    ByteBuffer(std::vector<uint8_t> &&);
    void write_string(std::string_view);
    void write_u8(uint8_t);
    void write_u16(uint16_t);
    void write_u32(uint32_t);
    void write_u64(uint64_t);
    const std::vector<uint8_t> data() const;

    std::optional<std::string> read_string(uint32_t);
    std::optional<uint8_t> read_u8();
    std::optional<uint16_t> read_u16();
    std::optional<uint32_t> read_u32();
    std::optional<uint64_t> read_u64();

private:
    std::vector<uint8_t> buffer_;
    uint64_t offset_;
};