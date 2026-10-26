#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include "persistence/ByteBuffer.h"

namespace
{
std::vector<uint8_t> bytes_of(const ByteBuffer &b)
{
    return std::vector<uint8_t>(b.data(), b.data() + b.size());
}
} // namespace

// ---------- writing ----------

TEST(ByteBufferTest, NewBufferIsEmpty)
{
    ByteBuffer b;
    EXPECT_EQ(b.size(), 0u);
    EXPECT_EQ(b.data(), nullptr);
    EXPECT_EQ(b.remaining_size(), 0u);
}

TEST(ByteBufferTest, WriteU8)
{
    ByteBuffer b;
    b.write_u8(0xAB);
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{0xAB}));
}

TEST(ByteBufferTest, WriteU16IsBigEndian)
{
    ByteBuffer b;
    b.write_u16(0x0102);
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{0x01, 0x02}));
}

TEST(ByteBufferTest, WriteU32IsBigEndian)
{
    ByteBuffer b;
    b.write_u32(0x01020304);
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}));
}

TEST(ByteBufferTest, WriteU64IsBigEndian)
{
    ByteBuffer b;
    b.write_u64(0x0102030405060708ULL);
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(ByteBufferTest, WriteStringAppendsRawBytes)
{
    ByteBuffer b;
    b.write_string("RDB1");
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{'R', 'D', 'B', '1'}));
}

TEST(ByteBufferTest, WriteBytesAppendsRawBytes)
{
    const uint8_t raw[] = {9, 8, 7};
    ByteBuffer b;
    b.write_bytes(raw, sizeof(raw));
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{9, 8, 7}));
}

TEST(ByteBufferTest, WritesAppendInOrder)
{
    ByteBuffer b;
    b.write_u8(1);
    b.write_u16(0x0203);
    b.write_string("A");
    EXPECT_EQ(bytes_of(b), (std::vector<uint8_t>{1, 2, 3, 'A'}));
}

// ---------- reading ----------

TEST(ByteBufferTest, IntegersRoundTrip)
{
    ByteBuffer b;
    b.write_u8(std::numeric_limits<uint8_t>::max());
    b.write_u16(std::numeric_limits<uint16_t>::max());
    b.write_u32(std::numeric_limits<uint32_t>::max());
    b.write_u64(std::numeric_limits<uint64_t>::max());
    b.write_u8(0);
    b.write_u16(0);
    b.write_u32(0);
    b.write_u64(0);

    EXPECT_EQ(b.read_u8(), std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(b.read_u16(), std::numeric_limits<uint16_t>::max());
    EXPECT_EQ(b.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(b.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(b.read_u8(), 0);
    EXPECT_EQ(b.read_u16(), 0);
    EXPECT_EQ(b.read_u32(), 0u);
    EXPECT_EQ(b.read_u64(), 0u);
    EXPECT_EQ(b.remaining_size(), 0u);
}

TEST(ByteBufferTest, StringRoundTripIsBinarySafe)
{
    std::string tricky("a\0b\xFF\x80z", 6);
    ByteBuffer b;
    b.write_string(tricky);

    auto result = b.read_string(static_cast<uint32_t>(tricky.size()));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, tricky);
}

TEST(ByteBufferTest, ReadStringOfLengthZeroReturnsEmptyString)
{
    ByteBuffer b; // even with nothing in it
    auto result = b.read_string(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "");
}

TEST(ByteBufferTest, ReadsAdvanceSequentiallyAndTrackRemaining)
{
    ByteBuffer b;
    b.write_u8(1);
    b.write_u32(2);
    b.write_string("xy");
    ASSERT_EQ(b.remaining_size(), 7u);

    EXPECT_EQ(b.read_u8(), 1);
    EXPECT_EQ(b.remaining_size(), 6u);
    EXPECT_EQ(b.read_u32(), 2u);
    EXPECT_EQ(b.remaining_size(), 2u);
    EXPECT_EQ(b.read_string(2), "xy");
    EXPECT_EQ(b.remaining_size(), 0u);
}

TEST(ByteBufferTest, ReadsPastTheEndReturnNullopt)
{
    ByteBuffer b;
    EXPECT_EQ(b.read_u8(), std::nullopt);
    EXPECT_EQ(b.read_u16(), std::nullopt);
    EXPECT_EQ(b.read_u32(), std::nullopt);
    EXPECT_EQ(b.read_u64(), std::nullopt);
    EXPECT_EQ(b.read_string(1), std::nullopt);
}

TEST(ByteBufferTest, ReadingAllBytesThenOneMoreFails)
{
    ByteBuffer b;
    b.write_u16(7);
    ASSERT_EQ(b.read_u16(), 7);
    EXPECT_EQ(b.read_u8(), std::nullopt);
}

TEST(ByteBufferTest, FailedReadDoesNotConsumeBytes)
{
    ByteBuffer b;
    b.write_u8(1);
    b.write_u16(0x0203); // 3 bytes total

    EXPECT_EQ(b.read_u32(), std::nullopt);
    EXPECT_EQ(b.remaining_size(), 3u);
    EXPECT_EQ(b.read_u8(), 1);
}

TEST(ByteBufferTest, ReadStringLongerThanBufferReturnsNullopt)
{
    ByteBuffer b;
    b.write_string("abc");
    EXPECT_EQ(b.read_string(4), std::nullopt);
    EXPECT_EQ(b.read_string(0xFFFFFFFFu), std::nullopt); // huge length must not overflow the bounds check
    EXPECT_EQ(b.remaining_size(), 3u);
}

TEST(ByteBufferTest, ConstructFromVectorStartsReadingAtTheFront)
{
    ByteBuffer b(std::vector<uint8_t>{0x00, 0x00, 0x01, 0x00, 'h', 'i'});

    EXPECT_EQ(b.size(), 6u);
    EXPECT_EQ(b.read_u32(), 256u);
    EXPECT_EQ(b.read_string(2), "hi");
}

TEST(ByteBufferTest, ReadingDoesNotChangeSizeOrData)
{
    ByteBuffer b;
    b.write_u32(0xDEADBEEF);
    auto before = bytes_of(b);

    b.read_u32();

    EXPECT_EQ(b.size(), 4u);
    EXPECT_EQ(bytes_of(b), before);
}

// ---------- crc32 ----------

TEST(ByteBufferTest, Crc32MatchesKnownCheckValue)
{
    // The standard CRC-32 check value for "123456789".
    ByteBuffer b;
    b.write_string("123456789");
    EXPECT_EQ(b.crc32(0, b.size() - 1), 0xCBF43926u);
}

TEST(ByteBufferTest, Crc32RangeEndIsInclusive)
{
    ByteBuffer whole;
    whole.write_string("123456789");
    ByteBuffer prefix;
    prefix.write_string("123");

    EXPECT_EQ(whole.crc32(0, 2), prefix.crc32(0, 2));
}

TEST(ByteBufferTest, Crc32CanStartMidBuffer)
{
    ByteBuffer whole;
    whole.write_string("xx123");
    ByteBuffer part;
    part.write_string("123");

    EXPECT_EQ(whole.crc32(2, 4), part.crc32(0, 2));
}

TEST(ByteBufferTest, Crc32DetectsASingleChangedByte)
{
    ByteBuffer a;
    a.write_string("hello world");
    ByteBuffer b;
    b.write_string("hello worle");

    EXPECT_NE(a.crc32(0, a.size() - 1), b.crc32(0, b.size() - 1));
}

TEST(ByteBufferTest, Crc32OfInvalidRangeIsTheEmptyCrc)
{
    ByteBuffer empty;
    EXPECT_EQ(empty.crc32(0, 0), 0u);

    ByteBuffer b;
    b.write_string("abc");
    EXPECT_EQ(b.crc32(2, 1), 0u);  // start > end
    EXPECT_EQ(b.crc32(0, 3), 0u);  // end past the last byte
}
