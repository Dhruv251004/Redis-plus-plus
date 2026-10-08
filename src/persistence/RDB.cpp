#include <fcntl.h>
#include <unistd.h>
#include "RDB.h"
#include "database/Database.h"
#include "utility/Types.h"
#include "constants/Constants.h"

RDB::RDB(const string &path) : path_(path) {}

bool RDB::write(const Database &db)
{
    ByteBuffer writer;

    // Serialize DB first
    writer.write_string("RDB1");              // HEADER
    writer.write_u8(static_cast<uint8_t>(1)); // VERSION
    writer.write_u64(db.get_db_size());       // Number of keys

    unordered_map<string, Database::KeyEntry> entry_copy = db.get_all_data();

    for (auto &[key, value] : entry_copy)
    {
        // Key info
        writer.write_u32(static_cast<uint32_t>(key.size()));
        writer.write_string(key);

        // Value info
        writer.write_u8(static_cast<uint8_t>(ValueType::String));

        string val = value.data;
        writer.write_u64(static_cast<uint64_t>(val.size()));
        writer.write_string(val);

        writer.write_u64(value.expiry);
    }

    writer.write_u32(writer.crc32(0, writer.size() - 1)); // write checksum

    // write in a tmp file then replace atomically
    string tmp_file_path = BASE_DB_PATH + "/" + TMP_RDB_FILE_NAME;
    int fd = open(tmp_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
        cerr << "failed to open dump file\n";
        return false;
    }

    size_t total_written = 0;
    size_t total_size = writer.size();

    while (total_written < total_size)
    {
        ssize_t res = ::write(fd, writer.data() + total_written, total_size - total_written);

        if (res <= 0)
        {
            cerr << "failed to write dump to file\n";
            close(fd);
            return false;
        }

        total_written += static_cast<size_t>(res);
    }

    if (fsync(fd) < 0)
    {
        cerr << "failed to save dump file\n";
        close(fd);
        return false;
    }
    if (close(fd) < 0)
    {
        cerr << "failed to close dump file\n";
        return false;
    }

    // atomically replace the old file
    if (rename(tmp_file_path.c_str(), path_.c_str()) == -1)
    {
        unlink(tmp_file_path.c_str());
        return false;
    }

    return true;
}

bool RDB::read(Database &db)
{
    int fd = open(path_.c_str(), O_RDONLY);
    if (fd == -1)
    {
        cerr << "Failed to open file\n";
        return false;
    }

    ByteBuffer reader;

    uint8_t buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = ::read(fd, buffer, sizeof(buffer))) > 0)
    {
        reader.write_bytes(buffer, static_cast<size_t>(bytes_read));
    }

    if (bytes_read < 0)
    {
        cerr << "Failed to read file\n";
        close(fd);
        return false;
    }

    if (bytes_read == 0) // in case of empty file just return true
        return true;

    if (close(fd) < 0)
    {
        cerr << "Failed to close file\n";
        return false;
    }

    // Validate before modifying the database
    if (!validate(reader))
        return false;

    // Skip header and version
    reader.read_string(4);
    reader.read_u8();

    auto num_keys = reader.read_u64();
    if (!num_keys.has_value())
        return false;

    for (uint64_t i = 0; i < *num_keys; ++i)
    {
        auto key_len = reader.read_u32();
        if (!key_len.has_value())
            return false;

        auto key = reader.read_string(*key_len);
        if (!key.has_value())
            return false;

        // Value type
        auto type = reader.read_u8();
        if (!type.has_value())
            return false;

        // Currently only supporting String
        auto value_len = reader.read_u64();
        if (!value_len.has_value())
            return false;

        auto value = reader.read_string(*value_len);
        if (!value.has_value())
            return false;

        auto expiry = reader.read_u64();
        if (!expiry.has_value())
            return false;

        if (*expiry == NO_EXPIRY)
            db.set(*key, *value);
        else
            db.set(*key, *value, *expiry);
    }

    return true;
}

bool RDB::validate(ByteBuffer content)
{
    if (content.size() < 4)
        return false;

    uint32_t checksum = content.crc32(0, content.size() - 4);
    auto header = content.read_string(4); // header ( 4 bytes )
    if (!header.has_value() || *header != "RDB1")
        return false;

    auto version = content.read_u8(); // version
    if (!version.has_value() || *version != 1)
        return false;

    auto num_keys = content.read_u64(); // num of keys
    if (!num_keys.has_value())
        return false;

    while (*num_keys > 0)
    {
        // Key info
        auto key_len = content.read_u32();
        if (!key_len.has_value())
            return false;
        auto key = content.read_string(*key_len);
        if (!key.has_value())
            return false;

        // Value info
        auto type = content.read_u8();
        if (!type.has_value() || !is_valid_value_type(*type))
            return false;
        auto value_len = content.read_u64();
        if (!value_len.has_value())
            return false;
        auto value = content.read_string(*value_len);
        if (!value.has_value())
            return false;
        auto expiry = content.read_u64();
        if (!expiry.has_value())
            return false;

        (*num_keys)--;
    }

    auto stored_checksum = content.read_u32();
    if (!stored_checksum.has_value() || *stored_checksum != checksum || content.remaining_size() != 0)
        return false;

    return true;
}