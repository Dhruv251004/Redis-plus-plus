#include <gtest/gtest.h>
#include "database/Database.h"

TEST(DatabaseTest, SetThenGetReturnsValue)
{
    Database db;
    db.set("name", "dhruv");

    auto result = db.get("name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "dhruv");
}

TEST(DatabaseTest, GetMissingKeyReturnsNullopt)
{
    Database db;
    EXPECT_EQ(db.get("nope"), std::nullopt);
}

TEST(DatabaseTest, SetOverwritesExistingValue)
{
    Database db;
    db.set("k", "one");
    db.set("k", "two");

    EXPECT_EQ(db.get("k"), "two");
    EXPECT_EQ(db.get_db_size(), 1u);
}

TEST(DatabaseTest, DelExistingKeyReturnsTrueAndRemovesIt)
{
    Database db;
    db.set("k", "v");

    EXPECT_TRUE(db.del("k"));
    EXPECT_FALSE(db.exists("k"));
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabaseTest, DelMissingKeyReturnsFalse)
{
    Database db;
    EXPECT_FALSE(db.del("ghost"));
}

TEST(DatabaseTest, ExpiredKeyIsTreatedAsMissingAndRemoved)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000); // expiry already in the past

    EXPECT_FALSE(db.exists("k"));
    EXPECT_EQ(db.get("k"), std::nullopt);
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabaseTest, TtlMissingKeyReturnsNullopt)
{
    Database db;
    EXPECT_EQ(db.ttl("ghost"), std::nullopt);
}

TEST(DatabaseTest, ExpireMissingKeyReturnsFalse)
{
    Database db;
    EXPECT_FALSE(db.expire("ghost", current_time_ms() + 1000));
}

TEST(DatabaseTest, ExpireUpdatesTtl)
{
    Database db;
    db.set("k", "v");
    int64_t when = current_time_ms() + 60'000;

    EXPECT_TRUE(db.expire("k", when));
    EXPECT_EQ(db.ttl("k"), when);
}

TEST(DatabaseTest, PersistWithoutExpiryReturnsFalse)
{
    Database db;
    db.set("k", "v");
    EXPECT_FALSE(db.persist("k"));
}

TEST(DatabaseTest, PersistRemovesExpiry)
{
    Database db;
    db.set("k", "v", current_time_ms() + 60'000);

    EXPECT_TRUE(db.persist("k"));
    EXPECT_EQ(db.ttl("k"), NO_EXPIRY);
}
