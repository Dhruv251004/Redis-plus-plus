#include <gtest/gtest.h>
#include <string>
#include "protocol/RespSerializer.h"
#include "protocol/RespParser.h"

// ---------- serialize(): one test per RESP type ----------

TEST(RespSerializerTest, SimpleString)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::simple_string("OK")), "+OK\r\n");
}

TEST(RespSerializerTest, Error)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::error("ERR boom")), "-ERR boom\r\n");
}

TEST(RespSerializerTest, Integer)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::integer(42)), ":42\r\n");
    EXPECT_EQ(RespSerializer::serialize(RespValue::integer(0)), ":0\r\n");
    EXPECT_EQ(RespSerializer::serialize(RespValue::integer(-7)), ":-7\r\n");
}

TEST(RespSerializerTest, BulkString)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::bulk_string("hello")), "$5\r\nhello\r\n");
}

TEST(RespSerializerTest, EmptyBulkString)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::bulk_string("")), "$0\r\n\r\n");
}

TEST(RespSerializerTest, BulkStringIsBinarySafe)
{
    std::string with_nul("a\0b", 3);
    EXPECT_EQ(RespSerializer::serialize(RespValue::bulk_string(with_nul)),
              std::string("$3\r\na\0b\r\n", 9));
}

TEST(RespSerializerTest, BulkStringLengthCountsBytesNotCrlf)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::bulk_string("a\r\nb")), "$4\r\na\r\nb\r\n");
}

TEST(RespSerializerTest, NullBulkString)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::null_bulk_string()), "$-1\r\n");
}

// ---------- arrays ----------

TEST(RespSerializerTest, EmptyArray)
{
    EXPECT_EQ(RespSerializer::serialize(RespValue::array({})), "*0\r\n");
}

TEST(RespSerializerTest, ArrayOfBulkStrings)
{
    auto value = RespValue::array({RespValue::bulk_string("GET"), RespValue::bulk_string("foo")});
    EXPECT_EQ(RespSerializer::serialize(value), "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
}

TEST(RespSerializerTest, ArrayMayMixTypes)
{
    auto value = RespValue::array({RespValue::integer(1),
                                   RespValue::simple_string("OK"),
                                   RespValue::null_bulk_string(),
                                   RespValue::error("ERR x")});
    EXPECT_EQ(RespSerializer::serialize(value), "*4\r\n:1\r\n+OK\r\n$-1\r\n-ERR x\r\n");
}

TEST(RespSerializerTest, NestedArrays)
{
    auto inner = RespValue::array({RespValue::integer(1), RespValue::integer(2)});
    auto outer = RespValue::array({inner, RespValue::bulk_string("x")});
    EXPECT_EQ(RespSerializer::serialize(outer), "*2\r\n*2\r\n:1\r\n:2\r\n$1\r\nx\r\n");
}

// ---------- fallback ----------

TEST(RespSerializerTest, DefaultConstructedValueSerializesAsError)
{
    // RespValue() has type Null, which has no wire format of its own.
    EXPECT_EQ(RespSerializer::serialize(RespValue()), "-ERR unsupported response type\r\n");
}

// ---------- the individual helpers ----------

TEST(RespSerializerTest, HelpersMatchSerialize)
{
    EXPECT_EQ(RespSerializer::serialize_simple_string("PONG"), "+PONG\r\n");
    EXPECT_EQ(RespSerializer::serialize_bulk_string("abc"), "$3\r\nabc\r\n");
    EXPECT_EQ(RespSerializer::serialize_error("ERR no"), "-ERR no\r\n");
    EXPECT_EQ(RespSerializer::serialize_null_bulk_string(), "$-1\r\n");
    EXPECT_EQ(RespSerializer::serialize_integer("99"), ":99\r\n");
}

// ---------- round trip with the parser ----------

TEST(RespSerializerTest, CommandArraySurvivesRoundTripThroughParser)
{
    // Clients send arrays of bulk strings; serializing one and parsing it
    // back should give the same arguments.
    std::vector<std::string> args = {"SET", "key", "", std::string("a\0b", 3), "line1\r\nline2"};

    std::vector<RespValue> items;
    for (const auto &a : args)
        items.push_back(RespValue::bulk_string(a));

    std::string wire = RespSerializer::serialize(RespValue::array(items));
    ParseResult parsed = RespParser::parse(wire);

    ASSERT_EQ(parsed.status, ParseStatus::Complete);
    EXPECT_EQ(parsed.bytes_consumed, wire.size());
    ASSERT_TRUE(parsed.value.has_value());

    const auto &children = parsed.value->children();
    ASSERT_EQ(children.size(), args.size());
    for (size_t i = 0; i < args.size(); i++)
        EXPECT_EQ(children[i].data(), args[i]);
}
