#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "debug/Inspector.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

class InspectorTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            debugging = require('haylen.debug')
            player = {speed = 220, name = 'Hero', alive = true, stats = {hp = 3}, items = {'axe', 'rope'}}
            camera = require('haylen.graphics2d').newCamera()
            debugging.watch('player', player)
            debugging.watch('camera', camera)
            function describe(path)
                local lines = {}
                for _, entry in ipairs(debugging.inspect(path)) do
                    lines[#lines + 1] = entry.key .. ':' .. entry.type .. ':' .. entry.value .. (entry.editable and ':edit' or '') .. (entry.expandable and ':open' or '')
                end
                return table.concat(lines, ' ')
            end
            function run(line)
                local ok, output = debugging.runCommand(line)
                return tostring(ok) .. ' ' .. output
            end
        )");
        // clang-format on
    }

    test::EngineFixture fixture;
};

TEST_F(InspectorTest, ListsTheRootsTablesAndThePropertiesOfBoundObjects) {
    EXPECT_TRUE(fixture.lua("return describe()").starts_with("watched:table:2 values:open scenes:table:Empty:open autoloads:table:Empty:open globals:table:")) << fixture.lua("return describe()");
    fixture.runLua("require('haylen.scene').push({name = 'play', score = 3})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return describe('scenes')"), "1:table:play, 2 values:open");
    EXPECT_EQ(fixture.lua("return describe('watched.player')"), "alive:boolean:true:edit items:table:2 values:open name:string:Hero:edit speed:number:220:edit stats:table:1 value:open");
    EXPECT_EQ(fixture.lua("return describe('watched.player.items')"), "1:string:axe:edit 2:string:rope:edit");
    const std::string camera = fixture.lua("return describe('watched.camera')");
    EXPECT_NE(camera.find("minZoom:number:"), std::string::npos);
    EXPECT_NE(camera.find(":edit"), std::string::npos);
    EXPECT_NE(camera.find("viewSize:haylen.Vec2:"), std::string::npos) << camera;
    EXPECT_EQ(camera.find("viewSize:haylen.Vec2::edit"), std::string::npos);
    EXPECT_EQ(fixture.lua("return run('inspect watched.player.stats')"), "true hp  number  3");
}

// A description reaches the editor as JSON, so it keeps whole characters of a long text and replaces the bytes of a string that are not UTF-8.
TEST_F(InspectorTest, DescribesEveryTextAsWholeCharacters) {
    fixture.runLua("debugging.watch('texts', {long = string.rep('\\u{E9}', 100), broken = 'a\\xFFb', ['\\xFE'] = 1})");
    const std::vector<std::string> path = Inspector::splitPath("watched.texts");
    Inspector::Listing listing;
    Inspector::list(fixture.engine().getPlugin<lua::ScriptPlugin>().getState(), path, listing);
    const std::vector<Inspector::Entry>& entries = listing.entries;
    ASSERT_EQ(entries.size(), 3U);

    for (const Inspector::Entry& entry : entries) {
        EXPECT_TRUE(core::Utf8::isValid(entry.key)) << entry.key;
        EXPECT_TRUE(core::Utf8::isValid(entry.value)) << entry.value;
        EXPECT_NO_THROW((void)core::Json({{"key", entry.key}, {"value", entry.value}}).dump());
    }
}

TEST_F(InspectorTest, EditsValuesThroughTheirSettersWithTheirChecks) {
    EXPECT_EQ(fixture.lua("return run('set watched.player.speed 300') .. ' ' .. player.speed .. ' ' .. math.type(player.speed)"), "true Set \"watched.player.speed\". 300 integer");
    EXPECT_EQ(fixture.lua("run('set watched.player.name Brave Hero') run('set watched.player.alive false') run('set watched.player.items.2 lamp') return player.name .. ' ' .. tostring(player.alive) .. ' ' .. player.items[2]"), "Brave Hero false lamp");
    EXPECT_EQ(fixture.lua("return run('set watched.player.speed fast')"), "false The value at \"watched.player.speed\" is a number, and \"fast\" is not one.");
    EXPECT_EQ(fixture.lua("return run('set watched.player.stats 1')"), "false The value at \"watched.player.stats\" is of the type \"table\", which the inspector does not edit. It edits numbers, booleans and strings.");
    EXPECT_EQ(fixture.lua("return run('set watched.camera.x 42') .. ' ' .. camera.x"), "true Set \"watched.camera.x\". 42.0");
    EXPECT_NE(fixture.lua("return run('set watched.camera.minZoom -1')").find("The smallest zoom must be above 0 and not above the largest zoom."), std::string::npos);
    EXPECT_NE(fixture.lua("return run('inspect nowhere')").find("The inspector has no root \"nowhere\"."), std::string::npos);
    EXPECT_NE(fixture.lua("return run('inspect watched.player.speed.deeper')").find("is of the type \"number\", which holds no \"deeper\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("run('set watched.player.items.1x knife') return player.items[1]"), "axe") << "A key that only starts with digits is a name.";
}

TEST_F(InspectorTest, HoldsWatchedValuesWeaklyAndStaysOffOutsideDevelopment) {
    fixture.runLua("debugging.watch('scratch', {1, 2, 3}) collectgarbage() collectgarbage()");
    EXPECT_EQ(fixture.lua("return describe('watched')").find("scratch"), std::string::npos);
    fixture.runLua("debugging.unwatch('camera')");
    EXPECT_EQ(fixture.lua("return describe('watched')"), "player:table:Hero, 5 values:open");
    EXPECT_NE(fixture.lua("debugging.setInspectorVisible(true)").find("The inspector is off outside development runs."), std::string::npos);
    EXPECT_FALSE(fixture.engine().getPlugin<plugins::DebugPlugin>().isInspectorVisible());
}

TEST(InspectorLuaTest, ShowsTheWindowInDevelopmentWithItsKey) {
    test::EngineFixture fixture({}, nullptr, {.development = true});
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    fixture.runLua("player = {speed = 1} require('haylen.debug').watch('player', player) require('haylen.debug').setInspectorVisible(true)");
    fixture.frames(3);
    EXPECT_TRUE(plugin.isInspectorVisible());
    EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('inspector false') return output"), "The inspector is hidden.");

    platform::Event key;
    key.type = platform::Event::Type::KeyDown;
    key.key = input::Key::F2;
    fixture.engine().handleEvent(key);
    EXPECT_TRUE(plugin.isInspectorVisible());
    fixture.runLua("require('haylen.debug').setInspectorKey('f6')");
    fixture.engine().handleEvent(key);
    EXPECT_TRUE(plugin.isInspectorVisible());
    EXPECT_EQ(fixture.lua("return require('haylen.debug').inspectorKey()"), "f6");
}

} // namespace haylen::debug
