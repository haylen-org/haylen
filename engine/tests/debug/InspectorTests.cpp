#include <gtest/gtest.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/EngineFixture.hpp"
#include "support/UiFixture.hpp"

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

class InspectorWindowTest : public ::testing::Test, public test::UiFixture {
  protected:
    InspectorWindowTest() : test::UiFixture({{"app.json", R"({"name": "Inspector", "identifier": "com.haylen.inspector", "debug": {"console": true}})"}}) {}

    // Turns a point of ImGui into the point of the visible area that the touches of the fixture take.
    [[nodiscard]] math::Vec2 place(ImVec2 point) {
        return getUi().getBackend().toDesign({point.x, point.y}) - getEngine().getViewport().getVisibleRect().getMin();
    }

    void swipe(math::Vec2 from, math::Vec2 to) {
        touch(platform::Event::Type::TouchBegan, 1, from);
        frames();
        for (int step = 1; step <= 8; ++step) {
            touch(platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / 8.0F));
            frames();
        }
        touch(platform::Event::Type::TouchEnded, 1, to);
        frames(2);
    }

    void tap(math::Vec2 point) {
        touch(platform::Event::Type::TouchBegan, 1, point);
        frames();
        touch(platform::Event::Type::TouchEnded, 1, point);
        frames(2);
    }
};

// A finger moves the window by its title bar, opens rows with a tap and scrolls the rows when it drags them, and the window stays inside the safe area.
TEST_F(InspectorWindowTest, FollowsAFingerAndStaysInsideTheSafeArea) {
    getFixture().runLua("values = {} for index = 1, 80 do values['value' .. index] = index end require('haylen.debug').watch('values', values) require('haylen.scene').push({}) require('haylen.debug').setInspectorVisible(true)");
    frames(2);
    ui::Backend& backend = getUi().getBackend();
    backend.makeCurrent();
    ImGuiWindow* window = ImGui::FindWindowByName("Inspector");
    ASSERT_NE(window, nullptr);
    const math::Rect safe = backend.getSafeRect();
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width);
    EXPECT_GE(window->Pos.y, safe.y);

    // The finger grabs the title bar near its right end and carries the window farther than the width of the window.
    const ImVec2 start = window->Pos;
    const math::Vec2 title = place({window->Pos.x + window->Size.x - 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    const math::Vec2 shift = place({start.x - window->Size.x - 20.0F, start.y + 10.0F}) - place(start);
    swipe(title, title + shift);
    EXPECT_NEAR(window->Pos.x, start.x - window->Size.x - 20.0F, 1.0F);
    EXPECT_NEAR(window->Pos.y, start.y + 10.0F, 1.0F);

    // A finger that drags the empty content under the rows leaves the window where it is.
    const ImVec2 kept = window->Pos;
    const math::Vec2 empty = place({window->Pos.x + 80.0F, window->Pos.y + window->Size.y - 40.0F});
    swipe(empty, empty + math::Vec2{-60.0F, -80.0F});
    EXPECT_EQ(window->Pos.x, kept.x);
    EXPECT_EQ(window->Pos.y, kept.y);

    // The first row is the root of the watched values, and the row under it the watched table.
    const float row = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.0F;
    const ImVec2 first{window->DC.CursorStartPos.x + 60.0F, window->DC.CursorStartPos.y + row * 0.5F};
    tap(place(first));
    tap(place({first.x, first.y + row}));
    ASSERT_GT(window->ScrollMax.y, 0.0F);

    const ImVec2 moved = window->Pos;
    const math::Vec2 rows = place({first.x, first.y + row * 6.0F});
    swipe(rows, place({first.x, first.y + row * 6.0F - 150.0F}));
    EXPECT_GT(window->Scroll.y, 100.0F);
    EXPECT_EQ(window->Pos.x, moved.x);
    EXPECT_EQ(window->Pos.y, moved.y);

    const math::Vec2 dragged = place({window->Pos.x + 80.0F, window->Pos.y + window->TitleBarHeight * 0.5F});
    swipe(dragged, dragged + math::Vec2{4000.0F, 4000.0F});
    EXPECT_LE(window->Pos.x + window->Size.x, safe.x + safe.width + 0.5F);
    EXPECT_LE(window->Pos.y + window->Size.y, safe.y + safe.height + 0.5F);
}

} // namespace haylen::debug
