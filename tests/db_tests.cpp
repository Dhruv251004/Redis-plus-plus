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

// ---------- active expiry: purge_expired ----------
// get_db_size() counts keys without expiring them, so these tests can tell a purge
// apart from the lazy removal that exists()/get() do.

TEST(DatabasePurgeTest, RemovesExpiredKeysWithoutAnyoneReadingThem)
{
    Database db;
    db.set("a", "1", current_time_ms() - 1000);
    db.set("b", "2", current_time_ms() - 1);
    ASSERT_EQ(db.get_db_size(), 2u);

    EXPECT_EQ(db.purge_expired(), 2u);
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabasePurgeTest, KeepsKeysThatHaveNotExpiredOrNeverExpire)
{
    Database db;
    db.set("past", "v", current_time_ms() - 1000);
    db.set("future", "v", current_time_ms() + 3'600'000);
    db.set("forever", "v");

    EXPECT_EQ(db.purge_expired(), 1u);
    EXPECT_EQ(db.get_db_size(), 2u);
    EXPECT_TRUE(db.exists("future"));
    EXPECT_TRUE(db.exists("forever"));
}

TEST(DatabasePurgeTest, EmptyDatabaseOrNothingDueRemovesNothing)
{
    Database db;
    EXPECT_EQ(db.purge_expired(), 0u);

    db.set("k", "v", current_time_ms() + 3'600'000);
    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_EQ(db.get_db_size(), 1u);
}

TEST(DatabasePurgeTest, RespectsTheMaxKeysLimit)
{
    Database db;
    for (int i = 0; i < 10; i++)
        db.set("k" + std::to_string(i), "v", current_time_ms() - 1000);

    EXPECT_EQ(db.purge_expired(4), 4u);
    EXPECT_EQ(db.get_db_size(), 6u);
    EXPECT_EQ(db.purge_expired(), 6u);
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabasePurgeTest, RemovesSoonestExpiringKeysFirst)
{
    Database db;
    int64_t now = current_time_ms();
    db.set("newest", "v", now - 10);
    db.set("oldest", "v", now - 3000);
    db.set("middle", "v", now - 1000);

    EXPECT_EQ(db.purge_expired(1), 1u);
    EXPECT_FALSE(db.get_all_data().count("oldest"));
    EXPECT_TRUE(db.get_all_data().count("middle"));
    EXPECT_TRUE(db.get_all_data().count("newest"));
}

TEST(DatabasePurgeTest, PlainSetClearsTheOldExpiry)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000);
    db.set("k", "v2"); // overwrite without expiry

    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_EQ(db.get("k"), "v2");
}

TEST(DatabasePurgeTest, OverwritingWithALaterExpiryMovesTheDeadline)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000);
    db.set("k", "v", current_time_ms() + 3'600'000);

    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_TRUE(db.exists("k"));
}

TEST(DatabasePurgeTest, PersistedKeyIsNotPurged)
{
    Database db;
    db.set("k", "v", current_time_ms() + 3'600'000);
    ASSERT_TRUE(db.persist("k"));
    // Even once time passes, a persisted key must not be in the index any more.
    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_EQ(db.ttl("k"), NO_EXPIRY);
}

TEST(DatabasePurgeTest, ExpireIntoThePastMakesTheKeyPurgeable)
{
    Database db;
    db.set("k", "v");
    ASSERT_TRUE(db.expire("k", current_time_ms() - 1000));

    EXPECT_EQ(db.purge_expired(), 1u);
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabasePurgeTest, ExpireMovesTheDeadlineOfALiveKey)
{
    Database db;
    int64_t later = current_time_ms() + 7'200'000;
    db.set("k", "v", current_time_ms() + 3'600'000);
    ASSERT_TRUE(db.expire("k", later));

    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_EQ(db.ttl("k"), later);
}

TEST(DatabasePurgeTest, DeletedKeyLeavesNothingBehindInTheIndex)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000);
    // del() first needs exists(), which lazily drops the expired key, so use a live one.
    db.set("live", "v", current_time_ms() + 3'600'000);
    ASSERT_TRUE(db.del("live"));
    db.set("live", "again"); // same key reused with no expiry

    EXPECT_EQ(db.purge_expired(), 1u); // only "k"
    EXPECT_EQ(db.get("live"), "again");
}

TEST(DatabasePurgeTest, LazilyRemovedKeyIsNotCountedAgain)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000);
    ASSERT_FALSE(db.exists("k")); // lazy removal happens here

    EXPECT_EQ(db.purge_expired(), 0u);
    EXPECT_EQ(db.get_db_size(), 0u);
}

TEST(DatabasePurgeTest, ManyKeysSharingOneDeadlineAreAllRemoved)
{
    Database db;
    int64_t when = current_time_ms() - 1000;
    for (int i = 0; i < 100; i++)
        db.set("k" + std::to_string(i), "v", when);

    EXPECT_EQ(db.purge_expired(), 100u);
    EXPECT_EQ(db.get_db_size(), 0u);
}

// ---------- EXPIRE must not revive a key that has already expired ----------

TEST(DatabaseTest, ExpireOnAnAlreadyExpiredKeyReturnsFalse)
{
    Database db;
    db.set("k", "v", current_time_ms() - 1000); // expired, not yet purged

    EXPECT_FALSE(db.expire("k", current_time_ms() + 3'600'000));
    EXPECT_FALSE(db.exists("k"));
}
