#include <gtest/gtest.h>

#include <string>

#include "haylen/core/Engine.hpp"
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
    EXPECT_EQ(fixture.lua("return describe()"), "watched:table::open scenes:table::open autoloads:table::open globals:table::open");
    EXPECT_EQ(fixture.lua("return describe('watched.player')"), "alive:boolean:true:edit items:table:2 items:open name:string:Hero:edit speed:number:220:edit stats:table:0 items:open");
    EXPECT_EQ(fixture.lua("return describe('watched.player.items')"), "1:string:axe:edit 2:string:rope:edit");
    const std::string camera = fixture.lua("return describe('watched.camera')");
    EXPECT_NE(camera.find("minZoom:number:"), std::string::npos);
    EXPECT_NE(camera.find(":edit"), std::string::npos);
    EXPECT_NE(camera.find("viewSize:haylen.Vec2:"), std::string::npos) << camera;
    EXPECT_EQ(camera.find("viewSize:haylen.Vec2::edit"), std::string::npos);
    EXPECT_EQ(fixture.lua("return run('inspect watched.player.stats')"), "true hp  number  3");
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
}

TEST_F(InspectorTest, HoldsWatchedValuesWeaklyAndStaysOffOutsideDevelopment) {
    fixture.runLua("debugging.watch('scratch', {1, 2, 3}) collectgarbage() collectgarbage()");
    EXPECT_EQ(fixture.lua("return describe('watched')").find("scratch"), std::string::npos);
    fixture.runLua("debugging.unwatch('camera')");
    EXPECT_EQ(fixture.lua("return describe('watched')"), "player:table:0 items:open");
    EXPECT_NE(fixture.lua("debugging.setInspectorVisible(true)").find("The inspector is off outside development runs."), std::string::npos);
    EXPECT_FALSE(fixture.engine().getPlugin<plugins::DebugPlugin>().isInspectorVisible());
}

TEST(InspectorLuaTest, ShowsTheWindowInDevelopment) {
    test::EngineFixture fixture({}, nullptr, {.development = true});
    fixture.runLua("player = {speed = 1} require('haylen.debug').watch('player', player) require('haylen.debug').setInspectorVisible(true)");
    fixture.frames(3);
    EXPECT_TRUE(fixture.engine().getPlugin<plugins::DebugPlugin>().isInspectorVisible());
    EXPECT_EQ(fixture.lua("local ok, output = require('haylen.debug').runCommand('inspector false') return output"), "The inspector is hidden.");
}

} // namespace haylen::debug
