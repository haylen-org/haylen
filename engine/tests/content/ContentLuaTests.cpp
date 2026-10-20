#include <gtest/gtest.h>

#include <map>
#include <string>

#include "support/EngineFixture.hpp"

namespace haylen::content {

// The Lua module of content in development, where the folder of the app plays and the files that `app.json` delivers later stay off the device until something brings them, as in a release.
class ContentLuaTest : public ::testing::Test {
  protected:
    ContentLuaTest() {
        // clang-format off
        fixture.runLua(R"(
            content = require('haylen.content')
            assets = require('haylen.assets')
            async = require('async')
        )");
        // clang-format on
    }

    test::EngineFixture fixture{{
        {"app.json", R"({"content": {"delivery": [{"match": "levels/**", "mode": "onDemand"}, {"match": "music/**", "mode": "prefetch"}]}})"},
        {"content/levels/forest.json", R"({"trees": 12})"},
        {"content/levels/desert.json", R"({"dunes": 3})"},
        {"content/music/theme.txt", "theme"},
        {"content/data/config.json", R"({"level": 1})"},
    }};
};

TEST_F(ContentLuaTest, TellsWhatPlaysAndWhatIsOnTheDevice) {
    EXPECT_EQ(fixture.lua("return tostring(content.isProtected()) .. ' ' .. content.generation() .. ' ' .. tostring(content.channel()) .. ' ' .. content.status()"), "false 0 nil idle");
    EXPECT_EQ(fixture.lua("return tostring(content.isLocal('data/config.json')) .. ' ' .. tostring(content.isLocal('levels/forest.json')) .. ' ' .. tostring(content.isLocal('music/theme.txt'))"), "true false false");
    EXPECT_EQ(fixture.lua("local progress = content.progress() return progress.receivedBytes .. ' ' .. progress.totalBytes"), "0 0");
    EXPECT_EQ(fixture.lua("return tostring(content.error()) .. ' ' .. tostring(content.update())"), "nil nil");
    EXPECT_EQ(fixture.lua("local stats = content.stats() return stats.generation .. ' ' .. stats.storedRecords"), "0 0");
    EXPECT_EQ(fixture.lua("local stats = content.stats() return stats.openShards .. ' ' .. stats.decodedChunks .. ' ' .. stats.activeReads .. ' ' .. stats.wantedRecords .. ' ' .. stats.downloadRate .. ' ' .. stats.reusedBytes .. ' ' .. stats.installedShards .. ' ' .. stats.remoteShards .. ' ' .. stats.missingChunks .. ' ' .. stats.pendingPrefetch"), "0 0 0 0 0.0 0 0 0 0 0");
}

TEST_F(ContentLuaTest, BringsAGroupWithoutLoadingIt) {
    // clang-format off
    fixture.runLua(R"(
        assets.defineGroup('forest', {'levels/'})
        local before = content.isGroupLocal('forest')
        async.spawn(function()
            local fractions = {}
            local done = content.prefetch('forest', function(progress) fractions[#fractions + 1] = progress.receivedBytes / progress.totalBytes end):await()
            result = tostring(done) .. ' ' .. #fractions .. ' ' .. tostring(content.isLocal('levels/desert.json')) .. ' ' .. tostring(before) .. ' ' .. tostring(content.isGroupLocal('forest'))
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return result") != "nil"; }));
    EXPECT_EQ(fixture.lua("return result"), "true 1 true false true");
    EXPECT_NE(fixture.lua("return content.isGroupLocal('desert')").find("The asset group \"desert\" is not defined."), std::string::npos);
    EXPECT_EQ(fixture.lua("return assets.json('levels/forest.json').trees"), "12");
}

// A file read off the frame thread comes down first when it downloads on demand, whole or by range, and a missing file rejects with the reason.
TEST_F(ContentLuaTest, ReadsTheBytesOfAFileOffTheFrameThread) {
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            local whole = assets.bytesAsync('levels/forest.json'):await()
            local part = assets.bytesAsync('levels/forest.json', 2, 5):await()
            local _, missing = assets.bytesAsync('levels/none.json'):await()
            result = whole .. '|' .. part .. '|' .. tostring(content.isLocal('levels/forest.json')) .. '|' .. tostring(missing)
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return result") != "nil"; }));
    const std::string result = fixture.lua("return result");
    EXPECT_EQ(result.substr(0, result.find("|true|")), R"({"trees": 12}|trees)");
    EXPECT_NE(result.find("none.json"), std::string::npos) << result;
}

TEST_F(ContentLuaTest, LoadsAnAssetThatDownloadsOnlyAsynchronously) {
    EXPECT_NE(fixture.lua("return assets.json('levels/forest.json')").find("is not on the device yet"), std::string::npos);
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            result = assets.loadAsync('levels/forest.json'):await().trees
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return result") != "nil"; }));
    EXPECT_EQ(fixture.lua("return result"), "12");
}

TEST_F(ContentLuaTest, ExplainsWhatDevelopmentCannotDo) {
    EXPECT_NE(fixture.lua("return content.check()").find("which has no updates"), std::string::npos);
    EXPECT_NE(fixture.lua("return content.apply()").find("No content update is ready"), std::string::npos);
    EXPECT_NE(fixture.lua("return content.prefetch('missing')").find("The asset group \"missing\" is not defined."), std::string::npos);
    EXPECT_NE(fixture.lua("return content.prefetch({'levels/forest.json'}, nil, {surprise = true})").find("Unknown option \"surprise\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return content.prefetch({'levels/none.json'})").find("does not exist"), std::string::npos);
    EXPECT_EQ(fixture.lua("content.addPackFolder('/no/such/folder') return 'The folder holds every pack.'"), "The folder holds every pack.");
}

} // namespace haylen::content
