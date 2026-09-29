#include <gtest/gtest.h>

#include <string>

#include "core/EmbeddedFiles.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::assets {

class AssetsLuaTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::map<std::string, std::string> getAssetFiles() {
        const std::vector<std::uint8_t> image = test::TestFiles::pngImage(8, 4, 0xFFFFFFFFU);
        const std::span<const std::uint8_t> font = core::EmbeddedFiles::getDefaultFont();
        return {
            {"content/images/hero.png", std::string(image.begin(), image.end())}, {"content/images/tree.png", std::string(image.begin(), image.end())}, {"content/fonts/ui.ttf", std::string(font.begin(), font.end())}, {"content/data/level.json", R"({"trees": [1, 2, 3]})"}, {"content/data/notes.txt", "notes"}, {"content/preload.json", R"({"groups": {"world": ["images/"], "broken": ["images/missing.png"]}})"},
        };
    }

    void SetUp() override {
        fixture.runLua("assets = require('haylen.assets') async = require('async')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    // Waits until the Lua expression becomes true while frames keep the asynchronous work moving.
    bool waitFor(const std::string& expression) {
        return fixture.frameUntil([&] { return lua("return " + expression) == "true"; });
    }

    test::EngineFixture fixture{getAssetFiles()};
};

TEST_F(AssetsLuaTest, LoadsAssetsSynchronously) {
    EXPECT_EQ(lua("return assets.texture('images/hero.png', {filter = 'linear'}).width"), "8");
    EXPECT_EQ(lua("return assets.font('fonts/ui.ttf', {bakeSize = 32}):lineHeight(16) > 0"), "true");
    EXPECT_EQ(lua("return #assets.json('data/level.json').trees"), "3");
    EXPECT_EQ(lua("return assets.text('data/notes.txt')"), "notes");
    EXPECT_EQ(lua("return assets.exists('data/notes.txt') and not assets.exists('data/missing.txt')"), "true");
    EXPECT_EQ(lua("return table.concat(assets.list('images'), ',')"), "images/hero.png,images/tree.png");
    EXPECT_EQ(lua("return #assets.list()"), "6");
    EXPECT_EQ(lua("return assets.load('images/hero.png').height .. ' ' .. #assets.load('data/level.json', 'json').trees"), "4 3");
    EXPECT_NE(lua("return assets.load('data/notes.txt')").find("No asset type handles the file 'data/notes.txt'."), std::string::npos);
    EXPECT_NE(lua("return assets.texture('images/missing.png')").find("The package file 'test/content/images/missing.png' was not found."), std::string::npos);
    EXPECT_EQ(lua("return type(assets.releaseUnused())"), "number");
}

TEST_F(AssetsLuaTest, InspectsTypesFilesAndTheCache) {
    EXPECT_EQ(lua("return assets.typeForPath('images/hero.png') .. ' ' .. assets.typeForPath('DATA/LEVEL.JSON')"), "texture json");
    EXPECT_NE(lua("return assets.typeForPath('data/notes.txt')").find("No asset type handles the file 'data/notes.txt'."), std::string::npos);
    EXPECT_EQ(lua("return tostring(assets.hasType('texture')) .. ' ' .. tostring(assets.hasType('mesh'))"), "true false");

    EXPECT_EQ(lua("local data = assets.bytes('images/hero.png') return #data .. ' ' .. data:sub(2, 4)"), std::to_string(test::TestFiles::pngImage(8, 4, 0xFFFFFFFFU).size()) + " PNG");
    EXPECT_EQ(lua("return assets.bytes('data/notes.txt')"), "notes");
    EXPECT_NE(lua("return assets.bytes('data/missing.bin')").find("error: "), std::string::npos);

    lua("base = assets.cachedCount() hero = assets.texture('images/hero.png') tree = assets.texture('images/tree.png')");
    EXPECT_EQ(lua("return assets.cachedCount() - base .. ' ' .. assets.pendingCount()"), "2 0");
    lua("pending = assets.loadAsync('data/level.json')");
    EXPECT_EQ(lua("return assets.pendingCount()"), "1");
    ASSERT_TRUE(waitFor("assets.pendingCount() == 0"));
    lua("hero = nil tree = nil pending = nil collectgarbage() collectgarbage() assets.releaseUnused()");
    EXPECT_EQ(lua("return assets.cachedCount() - base"), "0");
}

TEST_F(AssetsLuaTest, LoadsAssetsWithPromises) {
    // clang-format off
    lua(R"(
        done = false
        async.spawn(function()
            local texture = assets.loadAsync('images/hero.png'):await()
            local level = assets.loadAsync('data/level.json', 'json'):await()
            local missing, failure = assets.loadAsync('images/missing.png'):await()
            result = texture.width .. ' ' .. #level.trees .. ' ' .. tostring(missing) .. ' ' .. tostring(failure ~= nil)
            done = true
        end)
    )");
    // clang-format on
    ASSERT_TRUE(waitFor("done"));
    EXPECT_EQ(lua("return result"), "8 3 nil true");
}

TEST_F(AssetsLuaTest, PreloadsGroupsWithProgress) {
    // clang-format off
    lua(R"(
        assets.defineGroups('preload.json')
        assets.defineGroup('level', {'data/level.json', {path = 'fonts/ui.ttf', options = {bakeSize = 24}}})
        progress = {}
        async.spawn(function()
            local failures = assets.preload('world', function(fraction) progress[#progress + 1] = fraction end):await()
            local levelFailures = assets.preload('level'):await()
            local brokenFailures = assets.preload('broken'):await()
            summary = #failures .. ' ' .. #levelFailures .. ' ' .. #brokenFailures
        end)
    )");
    // clang-format on
    ASSERT_TRUE(waitFor("summary ~= nil"));
    EXPECT_EQ(lua("return summary"), "0 0 1");
    EXPECT_EQ(lua("return #progress .. ' ' .. progress[#progress]"), "2 1.0");
    EXPECT_EQ(lua("return table.concat(assets.groups(), ',')"), "broken,level,world");
    EXPECT_EQ(lua("return assets.groupLoaded('world') and assets.groupProgress('world') == 1"), "true");

    lua("assets.unloadGroup('world')");
    EXPECT_EQ(lua("return assets.groupLoaded('world') or assets.groupProgress('world') ~= 0"), "false");
    EXPECT_NE(lua("assets.preload('unknown')").find("The asset group 'unknown' is not defined."), std::string::npos);
    EXPECT_NE(lua("assets.unloadGroup('unknown')").find("The asset group 'unknown' is not defined."), std::string::npos);
}

TEST_F(AssetsLuaTest, ReportsFailingProgressCallbacks) {
    lua("assets.defineGroup('world', {'images/'}) assets.preload('world', function() error('progress failed') end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("progress failed"), std::string::npos);
}

} // namespace haylen::assets
