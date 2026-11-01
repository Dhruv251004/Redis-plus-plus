#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "command/CommandExecutor.h"
#include "server/ConnectionManager.h"
#include "pubsub/PubSubManager.h"
#include "protocol/RespSerializer.h"

namespace
{
constexpr int CLIENT_FD = 1;
constexpr int OTHER_FD = 2;

RespValue cmd(const std::vector<std::string> &parts)
{
    std::vector<RespValue> items;
    for (const auto &p : parts)
        items.push_back(RespValue::bulk_string(p));
    return RespValue::array(items);
}

void expect_simple(const RespValue &r, const std::string &text)
{
    EXPECT_EQ(r.type(), RespValue::Type::SimpleString);
    EXPECT_EQ(r.data(), text);
}

void expect_bulk(const RespValue &r, const std::string &text)
{
    EXPECT_EQ(r.type(), RespValue::Type::BulkString);
    EXPECT_EQ(r.data(), text);
}

void expect_int(const RespValue &r, int64_t n)
{
    EXPECT_EQ(r.type(), RespValue::Type::Integer);
    EXPECT_EQ(r.data(), std::to_string(n));
}

void expect_error(const RespValue &r, const std::string &prefix)
{
    ASSERT_TRUE(r.is_error());
    EXPECT_EQ(r.data().rfind(prefix, 0), 0u) << "error was: " << r.data();
}
} // namespace

class CommandExecutorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        conn_mgr.set_notify_handler([this](int fd, Connection &) { notified_fds.push_back(fd); });
        conn_mgr.add(CLIENT_FD);
        conn_mgr.add(OTHER_FD);
    }

    RespValue run(const std::vector<std::string> &parts)
    {
        return executor.execute(cmd(parts), conn_mgr.get(CLIENT_FD));
    }

    RespValue run_as_other(const std::vector<std::string> &parts)
    {
        return executor.execute(cmd(parts), conn_mgr.get(OTHER_FD));
    }

    Database db;
    ConnectionManager conn_mgr;
    PubSubManager pubsub{conn_mgr};
    CommandExecutor executor{db, pubsub};
    std::vector<int> notified_fds;
};

// ---------- dispatch / malformed requests ----------

TEST_F(CommandExecutorTest, NonArrayIsRejected)
{
    auto r = executor.execute(RespValue::simple_string("PING"), conn_mgr.get(CLIENT_FD));
    expect_error(r, "ERR expected array");
}

TEST_F(CommandExecutorTest, EmptyArrayIsRejected)
{
    expect_error(executor.execute(RespValue::array({}), conn_mgr.get(CLIENT_FD)), "ERR empty command");
}

TEST_F(CommandExecutorTest, NonBulkCommandNameIsRejected)
{
    auto r = executor.execute(RespValue::array({RespValue::integer(1)}), conn_mgr.get(CLIENT_FD));
    expect_error(r, "ERR invalid command name");
}

TEST_F(CommandExecutorTest, UnknownCommandNamesTheCommand)
{
    expect_error(run({"FLY"}), "ERR unknown command 'FLY'");
}

TEST_F(CommandExecutorTest, CommandNamesAreCaseInsensitive)
{
    expect_simple(run({"ping"}), "PONG");
    expect_simple(run({"PiNg"}), "PONG");
}

// ---------- PING / ECHO ----------

TEST_F(CommandExecutorTest, Ping)
{
    expect_simple(run({"PING"}), "PONG");
}

TEST_F(CommandExecutorTest, PingWithArgIsWrongArgCount)
{
    expect_error(run({"PING", "x"}), "ERR wrong number of arguments for 'PING'");
}

TEST_F(CommandExecutorTest, EchoReturnsArgument)
{
    expect_bulk(run({"ECHO", "hello"}), "hello");
}

TEST_F(CommandExecutorTest, EchoWithoutArgIsWrongArgCount)
{
    expect_error(run({"ECHO"}), "ERR wrong number of arguments for 'ECHO'");
}

// ---------- SET / GET ----------

TEST_F(CommandExecutorTest, SetThenGet)
{
    expect_simple(run({"SET", "k", "v"}), "OK");
    expect_bulk(run({"GET", "k"}), "v");
}

TEST_F(CommandExecutorTest, GetMissingKeyIsNullBulkString)
{
    EXPECT_EQ(run({"GET", "nope"}).type(), RespValue::Type::NullBulkString);
}

TEST_F(CommandExecutorTest, SetOverwrites)
{
    run({"SET", "k", "one"});
    run({"SET", "k", "two"});
    expect_bulk(run({"GET", "k"}), "two");
}

TEST_F(CommandExecutorTest, SetWithWrongArgCountIsRejected)
{
    expect_error(run({"SET", "k"}), "ERR wrong number of arguments for 'SET'");
    expect_error(run({"SET", "k", "v", "EX"}), "ERR wrong number of arguments for 'SET'");
}

TEST_F(CommandExecutorTest, SetEx)
{
    expect_simple(run({"SET", "k", "v", "EX", "100"}), "OK");

    auto ttl = run({"TTL", "k"});
    ASSERT_EQ(ttl.type(), RespValue::Type::Integer);
    EXPECT_GE(std::stoll(ttl.data()), 99);
    EXPECT_LE(std::stoll(ttl.data()), 100);
}

TEST_F(CommandExecutorTest, SetExIsCaseInsensitive)
{
    expect_simple(run({"SET", "k", "v", "ex", "100"}), "OK");
}

TEST_F(CommandExecutorTest, SetExWithNonIntegerIsRejected)
{
    expect_error(run({"SET", "k", "v", "EX", "abc"}), "ERR value is not an integer");
    EXPECT_FALSE(db.exists("k"));
}

TEST_F(CommandExecutorTest, SetWithUnknownOptionIsSyntaxError)
{
    expect_error(run({"SET", "k", "v", "PX", "100"}), "ERR syntax error");
    EXPECT_FALSE(db.exists("k"));
}

TEST_F(CommandExecutorTest, PlainSetClearsExistingExpiry)
{
    run({"SET", "k", "v", "EX", "100"});
    run({"SET", "k", "v2"});
    expect_int(run({"TTL", "k"}), -1);
}

TEST_F(CommandExecutorTest, GetWithWrongArgCountIsRejected)
{
    expect_error(run({"GET"}), "ERR wrong number of arguments for 'GET'");
    expect_error(run({"GET", "a", "b"}), "ERR wrong number of arguments for 'GET'");
}

// ---------- DEL / EXISTS ----------

TEST_F(CommandExecutorTest, DelReturnsOneThenZero)
{
    run({"SET", "k", "v"});
    expect_int(run({"DEL", "k"}), 1);
    expect_int(run({"DEL", "k"}), 0);
    EXPECT_EQ(run({"GET", "k"}).type(), RespValue::Type::NullBulkString);
}

TEST_F(CommandExecutorTest, Exists)
{
    expect_int(run({"EXISTS", "k"}), 0);
    run({"SET", "k", "v"});
    expect_int(run({"EXISTS", "k"}), 1);
}

// ---------- EXPIRE / TTL / PERSIST ----------

TEST_F(CommandExecutorTest, TtlMissingKeyIsMinusTwo)
{
    expect_int(run({"TTL", "nope"}), -2);
}

TEST_F(CommandExecutorTest, TtlWithoutExpiryIsMinusOne)
{
    run({"SET", "k", "v"});
    expect_int(run({"TTL", "k"}), -1);
}

TEST_F(CommandExecutorTest, ExpireSetsTtl)
{
    run({"SET", "k", "v"});
    expect_int(run({"EXPIRE", "k", "100"}), 1);

    auto ttl = std::stoll(run({"TTL", "k"}).data());
    EXPECT_GE(ttl, 99);
    EXPECT_LE(ttl, 100);
}

TEST_F(CommandExecutorTest, ExpireMissingKeyReturnsZero)
{
    expect_int(run({"EXPIRE", "nope", "100"}), 0);
}

TEST_F(CommandExecutorTest, ExpireWithNonIntegerIsRejected)
{
    run({"SET", "k", "v"});
    expect_error(run({"EXPIRE", "k", "soon"}), "ERR value is not an integer");
}

TEST_F(CommandExecutorTest, ExpireWithNegativeSecondsDeletesKey)
{
    run({"SET", "k", "v"});
    expect_int(run({"EXPIRE", "k", "-1"}), 1);
    expect_int(run({"EXISTS", "k"}), 0);
}

TEST_F(CommandExecutorTest, ExpireAtInThePastDeletesKey)
{
    run({"SET", "k", "v"});
    expect_int(run({"EXPIREAT", "k", "1"}), 1); // 1970-01-01T00:00:01Z
    expect_int(run({"EXISTS", "k"}), 0);
}

TEST_F(CommandExecutorTest, ExpireAtInTheFutureKeepsKey)
{
    run({"SET", "k", "v"});
    int64_t future_seconds = current_time_ms() / 1000 + 1000;
    expect_int(run({"EXPIREAT", "k", std::to_string(future_seconds)}), 1);
    expect_int(run({"EXISTS", "k"}), 1);
}

TEST_F(CommandExecutorTest, PexpireAtUsesMilliseconds)
{
    run({"SET", "k", "v"});
    expect_int(run({"PEXPIREAT", "k", "1"}), 1);
    expect_int(run({"EXISTS", "k"}), 0);
}

TEST_F(CommandExecutorTest, ExpireAtVariantsRejectNonIntegers)
{
    run({"SET", "k", "v"});
    expect_error(run({"EXPIREAT", "k", "x"}), "ERR value is not an integer");
    expect_error(run({"PEXPIREAT", "k", "x"}), "ERR value is not an integer");
}

TEST_F(CommandExecutorTest, PersistRemovesExpiry)
{
    run({"SET", "k", "v", "EX", "100"});
    expect_int(run({"PERSIST", "k"}), 1);
    expect_int(run({"TTL", "k"}), -1);
}

TEST_F(CommandExecutorTest, PersistWithoutExpiryOrMissingReturnsZero)
{
    run({"SET", "k", "v"});
    expect_int(run({"PERSIST", "k"}), 0);
    expect_int(run({"PERSIST", "nope"}), 0);
}

// ---------- transactions ----------

TEST_F(CommandExecutorTest, MultiExecRunsQueuedCommandsInOrder)
{
    expect_simple(run({"MULTI"}), "OK");
    expect_simple(run({"SET", "k", "v"}), "QUEUED");
    expect_simple(run({"GET", "k"}), "QUEUED");

    auto r = run({"EXEC"});
    ASSERT_EQ(r.type(), RespValue::Type::Array);
    ASSERT_EQ(r.children().size(), 2u);
    expect_simple(r.children()[0], "OK");
    expect_bulk(r.children()[1], "v");
}

TEST_F(CommandExecutorTest, QueuedCommandsDoNotRunUntilExec)
{
    run({"MULTI"});
    run({"SET", "k", "v"});
    EXPECT_FALSE(db.exists("k"));

    run({"EXEC"});
    EXPECT_TRUE(db.exists("k"));
}

TEST_F(CommandExecutorTest, ExecOnEmptyTransactionReturnsEmptyArray)
{
    run({"MULTI"});
    auto r = run({"EXEC"});
    ASSERT_EQ(r.type(), RespValue::Type::Array);
    EXPECT_TRUE(r.children().empty());
}

TEST_F(CommandExecutorTest, ExecWithoutMultiIsError)
{
    expect_error(run({"EXEC"}), "ERR EXEC without MULTI");
}

TEST_F(CommandExecutorTest, DiscardWithoutMultiIsError)
{
    expect_error(run({"DISCARD"}), "ERR DISCARD without MULTI");
}

TEST_F(CommandExecutorTest, DiscardDropsQueuedCommands)
{
    run({"MULTI"});
    run({"SET", "k", "v"});
    expect_simple(run({"DISCARD"}), "OK");

    EXPECT_FALSE(db.exists("k"));
    expect_error(run({"EXEC"}), "ERR EXEC without MULTI");
}

TEST_F(CommandExecutorTest, CommandsRunNormallyAfterExec)
{
    run({"MULTI"});
    run({"EXEC"});
    expect_simple(run({"PING"}), "PONG");
}

TEST_F(CommandExecutorTest, UnknownCommandInTransactionAbortsExec)
{
    run({"MULTI"});
    run({"SET", "k", "v"});
    expect_error(run({"FLY"}), "ERR unknown command 'FLY'");

    expect_error(run({"EXEC"}), "EXECABORT");
    EXPECT_FALSE(db.exists("k")); // nothing from the aborted transaction ran
}

TEST_F(CommandExecutorTest, TransactionWorksAgainAfterExecAbort)
{
    run({"MULTI"});
    run({"FLY"});
    run({"EXEC"}); // EXECABORT

    run({"MULTI"});
    run({"SET", "k", "v"});
    auto r = run({"EXEC"});

    ASSERT_EQ(r.type(), RespValue::Type::Array);
    EXPECT_TRUE(db.exists("k"));
}

TEST_F(CommandExecutorTest, TransactionsArePerConnection)
{
    run({"MULTI"});
    expect_simple(run_as_other({"SET", "k", "v"}), "OK"); // other client isn't in MULTI
    EXPECT_TRUE(db.exists("k"));
}

// ---------- pub/sub ----------

TEST_F(CommandExecutorTest, SubscribeReplyHasChannelAndCount)
{
    auto r = run({"SUBSCRIBE", "news"});

    ASSERT_EQ(r.type(), RespValue::Type::Array);
    ASSERT_EQ(r.children().size(), 3u);
    expect_bulk(r.children()[0], "subscribe");
    expect_bulk(r.children()[1], "news");
    expect_int(r.children()[2], 1);
}

TEST_F(CommandExecutorTest, SubscribeCountGrowsPerChannel)
{
    run({"SUBSCRIBE", "a"});
    auto r = run({"SUBSCRIBE", "b"});
    expect_int(r.children()[2], 2);
}

TEST_F(CommandExecutorTest, UnsubscribeReplyHasRemainingCount)
{
    run({"SUBSCRIBE", "a"});
    run({"SUBSCRIBE", "b"});
    auto r = run({"UNSUBSCRIBE", "a"});

    ASSERT_EQ(r.children().size(), 3u);
    expect_bulk(r.children()[0], "unsubscribe");
    expect_bulk(r.children()[1], "a");
    expect_int(r.children()[2], 1);
}

TEST_F(CommandExecutorTest, PublishWithNoSubscribersReturnsZero)
{
    expect_int(run({"PUBLISH", "news", "hi"}), 0);
}

TEST_F(CommandExecutorTest, PublishDeliversMessageToOtherSubscriber)
{
    run_as_other({"SUBSCRIBE", "news"});

    expect_int(run({"PUBLISH", "news", "hi"}), 1);

    std::string expected = RespSerializer::serialize(RespValue::array(
        {RespValue::bulk_string("message"), RespValue::bulk_string("news"), RespValue::bulk_string("hi")}));
    EXPECT_EQ(conn_mgr.get(OTHER_FD).write_buffer(), expected);
    EXPECT_EQ(notified_fds, std::vector<int>{OTHER_FD});
}

TEST_F(CommandExecutorTest, PublishDoesNotReachUnsubscribedChannel)
{
    run_as_other({"SUBSCRIBE", "sports"});
    expect_int(run({"PUBLISH", "news", "hi"}), 0);
    EXPECT_FALSE(conn_mgr.get(OTHER_FD).has_write_data());
}

TEST_F(CommandExecutorTest, PublishAfterUnsubscribeDeliversNothing)
{
    run_as_other({"SUBSCRIBE", "news"});
    run_as_other({"UNSUBSCRIBE", "news"});
    expect_int(run({"PUBLISH", "news", "hi"}), 0);
}

TEST_F(CommandExecutorTest, PublisherThatIsSubscribedGetsMessageWithoutNotify)
{
    // The publisher is flushed by the caller, so no notify event may fire for it.
    run({"SUBSCRIBE", "news"});

    expect_int(run({"PUBLISH", "news", "hi"}), 1);
    EXPECT_TRUE(conn_mgr.get(CLIENT_FD).has_write_data());
    EXPECT_TRUE(notified_fds.empty());
}

// ---------- write observer (feeds the AOF) ----------

namespace
{
std::string describe(const RespValue &command)
{
    std::string text;
    for (const auto &part : command.children())
        text += (text.empty() ? "" : " ") + part.data();
    return text;
}
} // namespace

class WriteObserverTest : public CommandExecutorTest
{
protected:
    void SetUp() override
    {
        CommandExecutorTest::SetUp();
        executor.set_write_observer([this](const RespValue &command, const RespValue &)
                                    { logged.push_back(describe(command)); });
    }

    std::vector<std::string> logged;
};

using Log = std::vector<std::string>;

TEST_F(WriteObserverTest, PlainWriteIsReported)
{
    run({"SET", "k", "v"});
    EXPECT_EQ(logged, (Log{"SET k v"}));
}

TEST_F(WriteObserverTest, EachEffectiveWriteCommandIsReported)
{
    run({"SET", "k", "v"});
    run({"EXPIRE", "k", "100"});
    run({"PERSIST", "k"});
    run({"EXPIREAT", "k", "9999999999"});
    run({"PEXPIREAT", "k", "9999999999000"});
    run({"DEL", "k"});

    EXPECT_EQ(logged, (Log{"SET k v", "EXPIRE k 100", "PERSIST k", "EXPIREAT k 9999999999",
                           "PEXPIREAT k 9999999999000", "DEL k"}));
}

TEST_F(WriteObserverTest, ReadOnlyCommandsAreNotReported)
{
    run({"SET", "k", "v"});
    logged.clear();

    run({"GET", "k"});
    run({"EXISTS", "k"});
    run({"TTL", "k"});
    run({"PING"});
    run({"ECHO", "x"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, FailedWritesAreNotReported)
{
    run({"SET", "k", "v", "EX", "abc"});
    run({"SET", "k"});
    run({"EXPIRE", "k", "soon"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, WritesThatChangeNothingAreNotReported)
{
    run({"DEL", "missing"});
    run({"EXPIRE", "missing", "100"});
    run({"PERSIST", "missing"});

    run({"SET", "k", "v"});
    logged.clear();
    run({"PERSIST", "k"}); // has no expiry to remove
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, ObserverRunsAfterTheDatabaseChanged)
{
    std::optional<std::string> seen;
    executor.set_write_observer([&](const RespValue &, const RespValue &)
                                { seen = db.get("k"); });

    run({"SET", "k", "v"});
    EXPECT_EQ(seen, "v");
}

// ---------- write observer: transactions ----------

TEST_F(WriteObserverTest, QueuedWritesAreReportedOnlyAtExecBracketedByMultiExec)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"SET", "b", "2"});
    EXPECT_TRUE(logged.empty());

    run({"EXEC"});
    EXPECT_EQ(logged, (Log{"MULTI", "SET a 1", "SET b 2", "EXEC"}));
}

TEST_F(WriteObserverTest, DiscardedTransactionReportsNothing)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"DISCARD"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, AbortedTransactionReportsNothing)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"FLY"});
    run({"EXEC"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, ReadOnlyTransactionReportsNothing)
{
    run({"SET", "k", "v"});
    logged.clear();

    run({"MULTI"});
    run({"GET", "k"});
    run({"EXEC"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, TransactionWhoseWritesAllFailReportsNothing)
{
    run({"MULTI"});
    run({"SET", "d", "4", "EX", "notint"});
    run({"DEL", "missing"});
    run({"EXEC"});
    EXPECT_TRUE(logged.empty());
}

TEST_F(WriteObserverTest, OnlyTheWritesThatSucceededInsideExecAreReported)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"SET", "b", "2", "EX", "notint"}); // fails when run
    run({"DEL", "missing"});                // changes nothing
    run({"EXPIRE", "a", "100"});
    run({"EXEC"});

    EXPECT_EQ(logged, (Log{"MULTI", "SET a 1", "EXPIRE a 100", "EXEC"}));
}

TEST_F(WriteObserverTest, EachTransactionIsBracketedSeparately)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"EXEC"});
    run({"MULTI"});
    run({"SET", "b", "2"});
    run({"EXEC"});

    EXPECT_EQ(logged, (Log{"MULTI", "SET a 1", "EXEC", "MULTI", "SET b 2", "EXEC"}));
}

TEST_F(WriteObserverTest, WriteAfterExecIsNotBracketed)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"EXEC"});
    logged.clear();

    run({"SET", "b", "2"});
    EXPECT_EQ(logged, (Log{"SET b 2"}));
}

TEST_F(WriteObserverTest, EmptyTransactionAfterABusyOneReportsNothing)
{
    run({"MULTI"});
    run({"SET", "a", "1"});
    run({"EXEC"});
    logged.clear();

    run({"MULTI"});
    run({"EXEC"});
    EXPECT_TRUE(logged.empty());
}
