#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>
#include "protocol/RespParser.h"

using namespace std::string_view_literals;

namespace
{
// Asserts the result is a complete array of bulk strings equal to `args`.
void expect_command(const ParseResult &r, const std::vector<std::string> &args, size_t bytes)
{
    ASSERT_EQ(r.status, ParseStatus::Complete);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.bytes_consumed, bytes);

    ASSERT_EQ(r.value->type(), RespValue::Type::Array);
    const auto &children = r.value->children();
    ASSERT_EQ(children.size(), args.size());
    for (size_t i = 0; i < args.size(); i++)
    {
        EXPECT_EQ(children[i].type(), RespValue::Type::BulkString);
        EXPECT_EQ(children[i].data(), args[i]);
    }
}
} // namespace

// ---------- parse(): valid input ----------

TEST(RespParserTest, ParsesSingleArgCommand)
{
    auto input = "*1\r\n$4\r\nPING\r\n"sv;
    expect_command(RespParser::parse(input), {"PING"}, input.size());
}

TEST(RespParserTest, ParsesMultiArgCommand)
{
    auto input = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n"sv;
    expect_command(RespParser::parse(input), {"SET", "key", "value"}, input.size());
}

TEST(RespParserTest, ParsesEmptyArray)
{
    auto input = "*0\r\n"sv;
    expect_command(RespParser::parse(input), {}, input.size());
}

TEST(RespParserTest, ParsesEmptyBulkString)
{
    auto input = "*1\r\n$0\r\n\r\n"sv;
    expect_command(RespParser::parse(input), {""}, input.size());
}

TEST(RespParserTest, ParsesBinarySafeBulkString)
{
    auto input = "*1\r\n$3\r\na\0b\r\n"sv; // embedded NUL
    expect_command(RespParser::parse(input), {std::string("a\0b", 3)}, input.size());
}

TEST(RespParserTest, BulkStringMayContainCrlf)
{
    // The declared length, not the content, decides where the string ends.
    auto input = "*1\r\n$4\r\na\r\nb\r\n"sv;
    expect_command(RespParser::parse(input), {"a\r\nb"}, input.size());
}

TEST(RespParserTest, PipelinedCommandsConsumeOnlyTheFirst)
{
    auto first = "*1\r\n$4\r\nPING\r\n"sv;
    std::string both = std::string(first) + "*2\r\n$3\r\nGET\r\n$1\r\nk\r\n";

    expect_command(RespParser::parse(both), {"PING"}, first.size());
}

// ---------- parse(): incomplete input ----------

TEST(RespParserTest, EmptyBufferIsIncomplete)
{
    auto r = RespParser::parse(""sv);
    EXPECT_EQ(r.status, ParseStatus::Incomplete);
    EXPECT_EQ(r.bytes_consumed, 0u);
}

TEST(RespParserTest, EveryProperPrefixIsIncomplete)
{
    // A TCP read can split a message anywhere, so each prefix must say "need more".
    const std::string full = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";

    for (size_t n = 0; n < full.size(); n++)
    {
        SCOPED_TRACE("prefix length " + std::to_string(n));
        auto r = RespParser::parse(std::string_view(full).substr(0, n));
        EXPECT_EQ(r.status, ParseStatus::Incomplete);
        EXPECT_EQ(r.bytes_consumed, 0u);
    }

    EXPECT_EQ(RespParser::parse(full).status, ParseStatus::Complete);
}

// ---------- parse(): invalid input ----------

TEST(RespParserTest, NonArrayIsInvalid)
{
    auto r = RespParser::parse("+OK\r\n"sv);
    EXPECT_EQ(r.status, ParseStatus::Invalid);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_TRUE(r.value->is_error());
}

TEST(RespParserTest, NonNumericArrayLengthIsInvalid)
{
    EXPECT_EQ(RespParser::parse("*x\r\n"sv).status, ParseStatus::Invalid);
}

TEST(RespParserTest, NegativeArrayLengthIsInvalid)
{
    EXPECT_EQ(RespParser::parse("*-1\r\n"sv).status, ParseStatus::Invalid);
}

TEST(RespParserTest, MissingCrlfAfterArrayLengthIsInvalid)
{
    EXPECT_EQ(RespParser::parse("*1\n$4\r\nPING\r\n"sv).status, ParseStatus::Invalid);
}

TEST(RespParserTest, NonBulkElementIsInvalid)
{
    EXPECT_EQ(RespParser::parse("*1\r\n:5\r\n"sv).status, ParseStatus::Invalid);
}

TEST(RespParserTest, NonNumericBulkLengthIsInvalid)
{
    EXPECT_EQ(RespParser::parse("*1\r\n$x\r\nPING\r\n"sv).status, ParseStatus::Invalid);
}

TEST(RespParserTest, WrongBulkLengthIsInvalid)
{
    // Declares 4 bytes but the bytes after "PING" aren't CRLF.
    EXPECT_EQ(RespParser::parse("*1\r\n$4\r\nPINGXX"sv).status, ParseStatus::Invalid);
}

// ---------- parse_bulk_string() ----------

TEST(RespParserTest, ParseBulkStringAdvancesPtr)
{
    auto input = "$5\r\nhello\r\n"sv;
    int ptr = 0;

    auto r = RespParser::parse_bulk_string(input, ptr);

    ASSERT_EQ(r.status, ParseStatus::Complete);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.value->type(), RespValue::Type::BulkString);
    EXPECT_EQ(r.value->data(), "hello");
    EXPECT_EQ(ptr, static_cast<int>(input.size()));
}

TEST(RespParserTest, ParseBulkStringRequiresDollar)
{
    int ptr = 0;
    EXPECT_EQ(RespParser::parse_bulk_string("5\r\nhello\r\n"sv, ptr).status, ParseStatus::Invalid);
}

// ---------- parse_simple_string() ----------

TEST(RespParserTest, ParsesSimpleString)
{
    auto input = "+OK\r\n"sv;
    auto r = RespParser::parse_simple_string(input);

    ASSERT_EQ(r.status, ParseStatus::Complete);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.value->type(), RespValue::Type::SimpleString);
    EXPECT_EQ(r.value->data(), "OK");
    EXPECT_EQ(r.bytes_consumed, input.size());
}

TEST(RespParserTest, SimpleStringWithoutCrlfIsIncomplete)
{
    EXPECT_EQ(RespParser::parse_simple_string("+OK"sv).status, ParseStatus::Incomplete);
    EXPECT_EQ(RespParser::parse_simple_string("+OK\r"sv).status, ParseStatus::Incomplete);
    EXPECT_EQ(RespParser::parse_simple_string(""sv).status, ParseStatus::Incomplete);
}

TEST(RespParserTest, SimpleStringWithoutPlusIsInvalid)
{
    EXPECT_EQ(RespParser::parse_simple_string("OK\r\n"sv).status, ParseStatus::Invalid);
}
