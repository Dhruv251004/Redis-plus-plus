#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "persistence/AOF.h"

namespace fs = std::filesystem;

class AofTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::string tmpl = (fs::temp_directory_path() / "redispp-aof-XXXXXX").string();
        ASSERT_NE(mkdtemp(tmpl.data()), nullptr);
        dir = tmpl;
        path = (dir / "test.aof").string();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    // Runs replay() and returns what the callback received (nullopt if it never ran).
    std::optional<std::string> replay_content(AOF &aof)
    {
        std::optional<std::string> result;
        aof.replay([&](const std::string &content) { result = content; });
        return result;
    }

    std::string file_contents()
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    fs::path dir;
    std::string path;
};

// ---------- open / close ----------

TEST_F(AofTest, OpenCreatesTheFile)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(AofTest, OpenFailsWhenDirectoryDoesNotExist)
{
    AOF aof((dir / "missing-dir" / "test.aof").string());
    EXPECT_FALSE(aof.open());
}

TEST_F(AofTest, CloseIsSafeToCallTwiceOrWithoutOpen)
{
    AOF never_opened(path);
    never_opened.close();

    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.close();
    aof.close();
}

TEST_F(AofTest, OperationsFailBeforeOpenAndAfterClose)
{
    AOF aof(path);
    EXPECT_FALSE(aof.append("x"));
    EXPECT_FALSE(aof.fsync());
    EXPECT_FALSE(aof.truncate());

    ASSERT_TRUE(aof.open());
    aof.close();
    EXPECT_FALSE(aof.append("x"));
    EXPECT_FALSE(aof.fsync());
    EXPECT_FALSE(aof.truncate());
}

// ---------- append ----------

TEST_F(AofTest, AppendWritesBytesToFile)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    EXPECT_TRUE(aof.append("*1\r\n$4\r\nPING\r\n"));

    EXPECT_EQ(file_contents(), "*1\r\n$4\r\nPING\r\n");
}

TEST_F(AofTest, AppendsAccumulateInOrder)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append("one|");
    aof.append("two|");
    aof.append("three");

    EXPECT_EQ(file_contents(), "one|two|three");
}

TEST_F(AofTest, AppendIsBinarySafe)
{
    std::string tricky("a\0b\r\n\xFF", 6);
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    ASSERT_TRUE(aof.append(tricky));

    EXPECT_EQ(file_contents(), tricky);
}

TEST_F(AofTest, ReopeningAppendsInsteadOfOverwriting)
{
    {
        AOF first(path);
        ASSERT_TRUE(first.open());
        first.append("first|");
        first.close();
    }

    AOF second(path);
    ASSERT_TRUE(second.open());
    second.append("second");

    EXPECT_EQ(file_contents(), "first|second");
}

TEST_F(AofTest, FsyncSucceedsOnOpenFile)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append("data");
    EXPECT_TRUE(aof.fsync());
}

// ---------- truncate ----------

TEST_F(AofTest, TruncateEmptiesTheFile)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append("old data");

    EXPECT_TRUE(aof.truncate());
    EXPECT_EQ(file_contents(), "");
}

TEST_F(AofTest, AppendAfterTruncateStartsAtTheBeginning)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append("old data");
    aof.truncate();
    aof.append("new");

    EXPECT_EQ(file_contents(), "new"); // no leading zero bytes / gap
}

// ---------- replay ----------

TEST_F(AofTest, ReplayOnMissingFileNeverCallsBack)
{
    AOF aof(path); // never opened, file doesn't exist
    EXPECT_EQ(replay_content(aof), std::nullopt);
}

TEST_F(AofTest, ReplayOnEmptyFileGivesEmptyContent)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());

    auto content = replay_content(aof);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, "");
}

TEST_F(AofTest, ReplayReturnsWhatWasAppended)
{
    AOF writer(path);
    ASSERT_TRUE(writer.open());
    writer.append("alpha|");
    writer.append("beta");

    EXPECT_EQ(replay_content(writer), "alpha|beta");
}

TEST_F(AofTest, ReplayWorksFromAFreshAofObject)
{
    {
        AOF writer(path);
        ASSERT_TRUE(writer.open());
        writer.append("persisted");
        writer.close();
    }

    AOF reader(path); // as after a server restart: never opened for writing
    EXPECT_EQ(replay_content(reader), "persisted");
}

TEST_F(AofTest, ReplayReadsFilesLargerThanItsInternalChunk)
{
    // replay reads in 4096-byte chunks; make sure every chunk is stitched together.
    std::string big;
    for (int i = 0; i < 10000; i++)
        big.push_back(static_cast<char>('a' + i % 26));

    AOF aof(path);
    ASSERT_TRUE(aof.open());
    ASSERT_TRUE(aof.append(big));

    auto content = replay_content(aof);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content->size(), big.size());
    EXPECT_EQ(*content, big);
}

TEST_F(AofTest, ReplayIsBinarySafe)
{
    std::string tricky("a\0b\r\n\xFF", 6);
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append(tricky);

    EXPECT_EQ(replay_content(aof), tricky);
}

TEST_F(AofTest, ReplayAfterTruncateIsEmpty)
{
    AOF aof(path);
    ASSERT_TRUE(aof.open());
    aof.append("gone");
    aof.truncate();

    EXPECT_EQ(replay_content(aof), "");
}
