#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "command/CommandExecutor.h"
#include "persistence/AofEntry.h"
#include "protocol/RespParser.h"
#include "protocol/RespSerializer.h"
#include "pubsub/PubSubManager.h"
#include "server/ConnectionManager.h"

namespace
{
RespValue cmd(const std::vector<std::string> &parts)
{
    std::vector<RespValue> items;
    for (const auto &p : parts)
        items.push_back(RespValue::bulk_string(p));
    return RespValue::array(items);
}

std::string wire(const std::vector<std::string> &parts)
{
    return RespSerializer::serialize(cmd(parts));
}
} // namespace

// ---------- aof_entry_for ----------

TEST(AofEntryTest, CommandsWithoutRelativeTimeAreLoggedAsReceived)
{
    Database db;
    for (const auto &parts : std::vector<std::vector<std::string>>{
             {"SET", "k", "v"}, {"DEL", "k"}, {"PERSIST", "k"}, {"EXPIREAT", "k", "9999999999"},
             {"PEXPIREAT", "k", "9999999999000"}, {"MULTI"}, {"EXEC"}})
    {
        EXPECT_EQ(aof_entry_for(cmd(parts), db), wire(parts)) << parts[0];
    }
}

TEST(AofEntryTest, SetWithExIsLoggedAsSetPlusAbsoluteDeadline)
{
    Database db;
    int64_t deadline = current_time_ms() + 60'000;
    db.set("k", "v", deadline); // state right after the command ran

    EXPECT_EQ(aof_entry_for(cmd({"SET", "k", "v", "EX", "60"}), db),
              wire({"SET", "k", "v"}) + wire({"PEXPIREAT", "k", std::to_string(deadline)}));
}

TEST(AofEntryTest, ExOptionIsMatchedCaseInsensitively)
{
    Database db;
    int64_t deadline = current_time_ms() + 60'000;
    db.set("k", "v", deadline);

    EXPECT_EQ(aof_entry_for(cmd({"set", "k", "v", "ex", "60"}), db),
              wire({"SET", "k", "v"}) + wire({"PEXPIREAT", "k", std::to_string(deadline)}));
}

TEST(AofEntryTest, SetWithExThatLeftNoExpiryIsLoggedAsPlainSet)
{
    Database db;
    db.set("k", "v"); // e.g. the seconds value overflowed to "never"

    EXPECT_EQ(aof_entry_for(cmd({"SET", "k", "v", "EX", "99"}), db), wire({"SET", "k", "v"}));
}

TEST(AofEntryTest, SetWithExIntoThePastIsLoggedAsDel)
{
    Database db; // the key was set and expired straight away, so it's gone
    EXPECT_EQ(aof_entry_for(cmd({"SET", "k", "v", "EX", "-5"}), db), wire({"DEL", "k"}));
}

TEST(AofEntryTest, ExpireIsLoggedAsAbsoluteDeadline)
{
    Database db;
    int64_t deadline = current_time_ms() + 60'000;
    db.set("k", "v", deadline);

    EXPECT_EQ(aof_entry_for(cmd({"EXPIRE", "k", "60"}), db),
              wire({"PEXPIREAT", "k", std::to_string(deadline)}));
}

TEST(AofEntryTest, ExpireThatDeletedTheKeyIsLoggedAsDel)
{
    Database db;
    EXPECT_EQ(aof_entry_for(cmd({"EXPIRE", "k", "-1"}), db), wire({"DEL", "k"}));
}

// ---------- run live, log, replay into a fresh server, compare ----------

namespace
{
struct Node
{
    Database db;
    ConnectionManager conn_mgr;
    PubSubManager pubsub{conn_mgr};
    CommandExecutor executor{db, pubsub};
    Connection &conn = (conn_mgr.add(1), conn_mgr.get(1));
};
} // namespace

class AofRoundTripTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        live.executor.set_write_observer([this](const RespValue &command, const RespValue &)
                                         { aof += aof_entry_for(command, live.db); });
    }

    void run(const std::vector<std::string> &parts)
    {
        live.executor.execute(cmd(parts), live.conn);
    }

    // same loop as RedisServer::handle_replay, without observer
    static void replay(const std::string &log, Node &into)
    {
        Connection replay_conn(-1);
        std::string_view rest = log;
        while (!rest.empty())
        {
            auto parsed = RespParser::parse(rest);
            if (parsed.status != ParseStatus::Complete)
                return;
            into.executor.execute(*parsed.value, replay_conn);
            rest.remove_prefix(parsed.bytes_consumed);
        }
    }

    void expect_replay_matches_live(const std::string &log)
    {
        Node restored;
        replay(log, restored);

        live.db.purge_expired();
        restored.db.purge_expired();
        auto expected = live.db.get_all_data();
        auto actual = restored.db.get_all_data();

        EXPECT_EQ(actual.size(), expected.size());
        for (const auto &[key, entry] : expected)
        {
            ASSERT_TRUE(actual.count(key)) << "missing after replay: " << key;
            EXPECT_EQ(actual[key].data, entry.data) << key;
            EXPECT_EQ(actual[key].expiry, entry.expiry) << key;
        }
    }

    Node live;
    std::string aof;
};

TEST_F(AofRoundTripTest, DiscardedTransactionIsNotLogged)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"DISCARD"});

    EXPECT_EQ(aof, "");
    expect_replay_matches_live(aof);
}

TEST_F(AofRoundTripTest, AbortedTransactionIsNotLogged)
{
    run({"MULTI"});
    run({"SET", "c", "3"});
    run({"FLY"});
    run({"EXEC"});

    EXPECT_EQ(aof, "");
    expect_replay_matches_live(aof);
}

TEST_F(AofRoundTripTest, QueuedWritesAreNotLoggedBeforeExec)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    EXPECT_EQ(aof, "");
}

TEST_F(AofRoundTripTest, CommittedTransactionKeepsItsExpiry)
{
    run({"MULTI"});
    run({"SET", "b", "2", "EX", "100"});
    run({"SET", "c", "3"});
    run({"EXEC"});

    expect_replay_matches_live(aof);

    Node restored;
    replay(aof, restored);
    EXPECT_NE(restored.db.ttl("b"), NO_EXPIRY);
    EXPECT_EQ(restored.db.ttl("c"), NO_EXPIRY);
}

TEST_F(AofRoundTripTest, WriteThatFailsInsideExecIsNotReplayed)
{
    run({"MULTI"});
    run({"SET", "d", "4", "EX", "notint"});
    run({"SET", "e", "5"});
    run({"EXEC"});

    Node restored;
    replay(aof, restored);
    EXPECT_FALSE(restored.db.exists("d"));
    EXPECT_EQ(restored.db.get("e"), "5");
    expect_replay_matches_live(aof);
}

TEST_F(AofRoundTripTest, ExpireIntoThePastDoesNotResurrectTheKey)
{
    run({"SET", "k", "v"});
    run({"EXPIRE", "k", "-1"});

    expect_replay_matches_live(aof);

    Node restored;
    replay(aof, restored);
    EXPECT_FALSE(restored.db.exists("k"));
}

TEST_F(AofRoundTripTest, SetExIntoThePastDoesNotResurrectTheKey)
{
    run({"SET", "k", "old"});
    run({"SET", "k", "new", "EX", "-1"});

    Node restored;
    replay(aof, restored);
    EXPECT_FALSE(restored.db.exists("k"));
    expect_replay_matches_live(aof);
}

TEST_F(AofRoundTripTest, MixedWorkloadReplaysToTheSameDatabase)
{
    run({"SET", "a", "1"});
    run({"SET", "b", "2", "EX", "100"});
    run({"EXPIRE", "a", "50"});
    run({"PERSIST", "b"});
    run({"DEL", "missing"});
    run({"MULTI"});
    run({"DEL", "a"});
    run({"SET", "z", "26"});
    run({"EXPIRE", "z", "10"});
    run({"EXEC"});
    run({"SET", "after", "tx"});

    expect_replay_matches_live(aof);
}

TEST_F(AofRoundTripTest, TransactionCutOffBeforeExecIsNotApplied)
{
    run({"SET", "pre", "1"});
    run({"MULTI"});
    run({"SET", "x", "1"});
    run({"SET", "y", "2"});
    run({"EXEC"});

    std::string crashed = aof.substr(0, aof.size() - wire({"EXEC"}).size()); // crash before EXEC was written

    Node restored;
    replay(crashed, restored);
    EXPECT_EQ(restored.db.get("pre"), "1");
    EXPECT_FALSE(restored.db.exists("x"));
    EXPECT_FALSE(restored.db.exists("y"));
}

TEST_F(AofRoundTripTest, CompleteTransactionIsApplied)
{
    run({"MULTI"});
    run({"SET", "x", "1"});
    run({"SET", "y", "2"});
    run({"EXEC"});

    Node restored;
    replay(aof, restored);
    EXPECT_EQ(restored.db.get("x"), "1");
    EXPECT_EQ(restored.db.get("y"), "2");
}
