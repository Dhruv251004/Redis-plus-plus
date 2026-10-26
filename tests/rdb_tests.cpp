#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "persistence/RDB.h"
#include "utility/Types.h"

namespace fs = std::filesystem;

namespace
{
// ---- helpers that build dump files byte by byte, mirroring the format in RDB.h ----

ByteBuffer dump_header(uint64_t declared_keys, uint8_t version = 1, const std::string &magic = "RDB1")
{
    ByteBuffer b;
    b.write_string(magic);
    b.write_u8(version);
    b.write_u64(declared_keys);
    return b;
}

void add_entry(ByteBuffer &b, const std::string &key, const std::string &value,
               int64_t expiry = NO_EXPIRY, uint8_t type = static_cast<uint8_t>(ValueType::String))
{
    b.write_u32(static_cast<uint32_t>(key.size()));
    b.write_string(key);
    b.write_u8(type);
    b.write_u64(value.size());
    b.write_string(value);
    b.write_u64(static_cast<uint64_t>(expiry));
}

void seal(ByteBuffer &b, int checksum_offset = 0)
{
    b.write_u32(b.crc32(0, b.size() - 1) + checksum_offset);
}

void expect_same_data(const Database &expected, const Database &actual)
{
    auto e = expected.get_all_data();
    auto a = actual.get_all_data();
    EXPECT_EQ(e.size(), a.size());
    for (const auto &[key, entry] : e)
    {
        auto it = a.find(key);
        ASSERT_NE(it, a.end()) << "missing key: " << key;
        EXPECT_EQ(it->second.data, entry.data) << "key: " << key;
        EXPECT_EQ(it->second.expiry, entry.expiry) << "key: " << key;
    }
}
} // namespace

// RDB::write stages its output in "data/dump-tmp.rdb" relative to the working directory,
// so each test runs inside a throwaway directory that has a data/ folder. That also keeps
// the tests away from the real data/dump.rdb.
class RdbTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        spdlog::set_level(spdlog::level::off);

        original_cwd = fs::current_path();
        std::string tmpl = (fs::temp_directory_path() / "redispp-rdb-XXXXXX").string();
        ASSERT_NE(mkdtemp(tmpl.data()), nullptr);
        dir = tmpl;
        fs::create_directory(dir / "data");
        fs::current_path(dir);
    }

    void TearDown() override
    {
        fs::current_path(original_cwd);
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    std::string read_file()
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    void write_file(const std::string &content)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void write_file(const ByteBuffer &b)
    {
        write_file(std::string(reinterpret_cast<const char *>(b.data()), b.size()));
    }

    fs::path original_cwd;
    fs::path dir;
    std::string path = "data/test.rdb";
};

// ---------- write + read round trips ----------

TEST_F(RdbTest, RoundTripPreservesKeysAndValues)
{
    Database original;
    original.set("name", "dhruv");
    original.set("city", "pune");
    original.set("empty-value", "");

    ASSERT_TRUE(RDB(path).write(original));

    Database loaded;
    ASSERT_TRUE(RDB(path).read(loaded));
    expect_same_data(original, loaded);
}

TEST_F(RdbTest, RoundTripPreservesExpiry)
{
    Database original;
    original.set("temporary", "v", current_time_ms() + 3'600'000);
    original.set("forever", "v");

    ASSERT_TRUE(RDB(path).write(original));

    Database loaded;
    ASSERT_TRUE(RDB(path).read(loaded));
    expect_same_data(original, loaded);
    EXPECT_EQ(loaded.ttl("forever"), NO_EXPIRY);
    EXPECT_NE(loaded.ttl("temporary"), NO_EXPIRY);
}

TEST_F(RdbTest, RoundTripIsBinarySafe)
{
    Database original;
    original.set(std::string("k\0ey", 4), std::string("v\0al\r\n\xFF", 8));
    original.set("", "empty key");

    ASSERT_TRUE(RDB(path).write(original));

    Database loaded;
    ASSERT_TRUE(RDB(path).read(loaded));
    expect_same_data(original, loaded);
}

TEST_F(RdbTest, RoundTripWithManyKeys)
{
    Database original;
    for (int i = 0; i < 1000; i++)
        original.set("key" + std::to_string(i), std::string(i % 50, 'x'));

    ASSERT_TRUE(RDB(path).write(original));

    Database loaded;
    ASSERT_TRUE(RDB(path).read(loaded));
    EXPECT_EQ(loaded.get_db_size(), 1000u);
    expect_same_data(original, loaded);
}

TEST_F(RdbTest, EmptyDatabaseRoundTrips)
{
    Database original;
    ASSERT_TRUE(RDB(path).write(original));

    // magic(4) + version(1) + count(8) + checksum(4)
    EXPECT_EQ(read_file().size(), 17u);

    Database loaded;
    EXPECT_TRUE(RDB(path).read(loaded));
    EXPECT_EQ(loaded.get_db_size(), 0u);
}

TEST_F(RdbTest, WriteStartsWithMagicAndVersion)
{
    Database db;
    db.set("k", "v");
    ASSERT_TRUE(RDB(path).write(db));

    std::string file = read_file();
    ASSERT_GE(file.size(), 13u);
    EXPECT_EQ(file.substr(0, 4), "RDB1");
    EXPECT_EQ(static_cast<uint8_t>(file[4]), 1);
}

TEST_F(RdbTest, WriteReplacesThePreviousDump)
{
    Database first;
    first.set("old", "1");
    ASSERT_TRUE(RDB(path).write(first));

    Database second;
    second.set("new", "2");
    ASSERT_TRUE(RDB(path).write(second));

    Database loaded;
    ASSERT_TRUE(RDB(path).read(loaded));
    expect_same_data(second, loaded);
    EXPECT_FALSE(loaded.exists("old"));
}

TEST_F(RdbTest, WriteLeavesNoTempFileBehind)
{
    Database db;
    db.set("k", "v");
    ASSERT_TRUE(RDB(path).write(db));

    EXPECT_FALSE(fs::exists("data/dump-tmp.rdb"));
    EXPECT_TRUE(fs::exists(path));
}

// ---------- read: files that are absent or empty ----------

TEST_F(RdbTest, ReadMissingFileSucceedsWithEmptyDatabase)
{
    Database db;
    EXPECT_TRUE(RDB(path).read(db));
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST_F(RdbTest, ReadEmptyFileSucceedsWithEmptyDatabase)
{
    write_file(std::string());

    Database db;
    EXPECT_TRUE(RDB(path).read(db));
    EXPECT_EQ(db.get_db_size(), 0u);
}

// ---------- read: damaged files are rejected and leave the database alone ----------

class RdbCorruptionTest : public RdbTest
{
protected:
    // A valid dump of one key ("k" -> "v"): 13 header bytes, 23 entry bytes, 4 checksum bytes.
    std::string valid_dump()
    {
        Database db;
        db.set("k", "v");
        EXPECT_TRUE(RDB(path).write(db));
        std::string file = read_file();
        EXPECT_EQ(file.size(), 40u);
        return file;
    }

    void expect_rejected_and_db_untouched(const std::string &bad_file)
    {
        write_file(bad_file);

        Database db;
        db.set("existing", "value");
        EXPECT_FALSE(RDB(path).read(db));

        EXPECT_EQ(db.get_db_size(), 1u);
        EXPECT_EQ(db.get("existing"), "value");
        EXPECT_FALSE(db.exists("k"));
    }
};

TEST_F(RdbCorruptionTest, FlippedByteInValueFailsChecksum)
{
    std::string file = valid_dump();
    file[27] ^= 0x01; // the single value byte
    expect_rejected_and_db_untouched(file);
}

TEST_F(RdbCorruptionTest, FlippedByteInChecksumIsRejected)
{
    std::string file = valid_dump();
    file[file.size() - 1] ^= 0x01;
    expect_rejected_and_db_untouched(file);
}

TEST_F(RdbCorruptionTest, WrongMagicIsRejected)
{
    std::string file = valid_dump();
    file[0] = 'X';
    expect_rejected_and_db_untouched(file);
}

TEST_F(RdbCorruptionTest, TruncatedFileIsRejectedAtEveryLength)
{
    std::string file = valid_dump();

    for (size_t len = 1; len < file.size(); len++)
    {
        SCOPED_TRACE("truncated to " + std::to_string(len) + " bytes");
        expect_rejected_and_db_untouched(file.substr(0, len));
    }
}

TEST_F(RdbCorruptionTest, TrailingBytesAreRejected)
{
    expect_rejected_and_db_untouched(valid_dump() + "junk");
}

// ---------- validate() on hand-built dumps ----------

TEST_F(RdbTest, ValidateAcceptsWellFormedDump)
{
    ByteBuffer b = dump_header(2);
    add_entry(b, "a", "1");
    add_entry(b, "b", "2", current_time_ms() + 1000);
    seal(b);

    EXPECT_TRUE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateAcceptsDumpWithZeroKeys)
{
    ByteBuffer b = dump_header(0);
    seal(b);

    EXPECT_TRUE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsBufferSmallerThanMagic)
{
    ByteBuffer b(std::vector<uint8_t>{'R', 'D', 'B'});
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsMagicWithNothingAfterIt)
{
    ByteBuffer b(std::vector<uint8_t>{'R', 'D', 'B', '1'});
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsWrongMagicEvenWithValidChecksum)
{
    ByteBuffer b = dump_header(0, 1, "NOPE");
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsUnsupportedVersionEvenWithValidChecksum)
{
    ByteBuffer b = dump_header(0, 2);
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsUnknownValueType)
{
    ByteBuffer b = dump_header(1);
    add_entry(b, "k", "v", NO_EXPIRY, 99);
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsWrongChecksum)
{
    ByteBuffer b = dump_header(1);
    add_entry(b, "k", "v");
    seal(b, 1);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsMissingChecksum)
{
    ByteBuffer b = dump_header(1);
    add_entry(b, "k", "v"); // not sealed
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsKeyCountLargerThanEntries)
{
    ByteBuffer b = dump_header(3);
    add_entry(b, "k", "v");
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsKeyCountSmallerThanEntries)
{
    ByteBuffer b = dump_header(1);
    add_entry(b, "a", "1");
    add_entry(b, "b", "2");
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}

TEST_F(RdbTest, ValidateRejectsValueLengthLargerThanRemainingBytes)
{
    ByteBuffer b = dump_header(1);
    b.write_u32(1);
    b.write_string("k");
    b.write_u8(static_cast<uint8_t>(ValueType::String));
    b.write_u64(1000); // claims 1000 bytes, supplies 1
    b.write_string("v");
    seal(b);
    EXPECT_FALSE(RDB(path).validate(b));
}
