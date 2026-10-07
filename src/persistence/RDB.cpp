#include <fcntl.h>
#include <unistd.h>
#include "RDB.h"
#include "database/Database.h"
#include "utility/Types.h"

RDB::RDB(const string &path) : path_(path) {}

bool RDB::write(const Database &db)
{
    ByteBuffer write_content;

    // Serialize DB first
    write_content.write_string("RDB1");              // HEADER
    write_content.write_u8(static_cast<uint8_t>(1)); // VERSION
    write_content.write_u64(db.get_db_size());       // Number of keys

    unordered_map<string, Database::KeyEntry> entry_copy = db.get_all_data();

    for (auto &[key, value] : entry_copy)
    {
        // Key info
        write_content.write_u32(static_cast<uint32_t>(key.size()));
        write_content.write_string(key);

        // Value info
        write_content.write_u8(static_cast<uint8_t>(ValueType::String));

        string val = value.data;
        write_content.write_u64(static_cast<uint64_t>(val.size()));
        write_content.write_string(val);

        write_content.write_u64(value.expiry);
    }

    write_content.write_u32(write_content.crc32()); // write checksum

    int fd = open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
        cerr << "failed to open dump file\n";
        return false;
    }

    size_t total_written = 0;
    size_t total_size = write_content.size();

    while (total_written < total_size)
    {
        ssize_t res = ::write(fd, write_content.data() + total_written, total_size - total_written);

        if (res <= 0)
        {
            cerr << "failed to write dump to file\n";
            close(fd);
            return false;
        }

        total_written += static_cast<size_t>(res);
    }

    if (close(fd) < 0)
    {
        cerr << "failed to close dump file\n";
        return false;
    }

    return true;
}
