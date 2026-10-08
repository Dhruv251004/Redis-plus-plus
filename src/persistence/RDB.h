#pragma once
#include "ByteBuffer.h"
#include "database/Database.h"
#include <iostream>

/*
 * RDB File Format
 *
 * All multi-byte integers are encoded in big-endian (MSB-first) order.
 *
 * Header:
 *   [magic: u32]
 *   [version: u8]
 *   [key_count: u64]
 *
 * Each key-value entry:
 *   [key_length: u64]
 *   [key: key_length bytes]
 *
 *   [type: u8]
 *
 *   If type == String:
 *       [value_length: u64]
 *       [value: value_length bytes]
 *
 *   If type == List:
 *       [element_count: u64]
 *       For each element:
 *           [element_length: u64]
 *           [element: element_length bytes]
 *
 *   If type == Set:
 *       [element_count: u64]
 *       For each element:
 *           [element_length: u64]
 *           [element: element_length bytes]
 *
 *   [expiry: i64]
 *
 * Footer:
 *   [checksum: u32]
 *
 * Notes:
 *   - Lengths represent the number of bytes, not characters.
 *   - Collection counts represent the number of elements.
 *   - Integer widths are fixed by the format.
 *   - The format must not depend on the platform's native integer size
 *     or endianness.
 */

class RDB
{
public:
    RDB(const string &);
    bool write(const Database &);
    bool read(Database &);
    bool validate(ByteBuffer);

private:
    std::string path_;
};