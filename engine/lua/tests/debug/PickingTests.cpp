#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <numbers>
#include <sstream>
#include <string>

#include "debug/SelectionOverlay.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "support/LuaUiFixture.hpp"

namespace haylen::lua::debug {

// Runs an app in development whose scene draws a sprite and a tree of nodes through a zoomed camera, with the inspector open and the pick on the screen on, and presses and drags on the screen with a finger.
class PickingTest : public ::testing::Test, public test::LuaUiFixture {
  protected:
    PickingTest() : test::LuaUiFixture({{"haylen.json", R"({"name": "Picking", "identifier": "com.haylen.picking"})"}}, {.development = true}) {}

    void SetUp() override {
        // clang-format off
        getFixture().runLua(R"(
            debug = require('haylen.debug')
            graphics = require('haylen.graphics')
            graphics2d = require('haylen.graphics2d')
            nodes2d = require('haylen.nodes2d')
            camera = graphics2d.newCamera()
            camera.zoom = {2, 2}
            box = graphics2d.newSprite(graphics.whiteTexture(), {x = 40, y = 20, width = 40, height = 20})
            tree = nodes2d.newTree()
            parent = tree:add({name = 'parent', x = -60, y = 30, scaleX = 2, scaleY = 2})
            child = parent:add({name = 'child', x = 10, sprite = graphics2d.newSprite(graphics.whiteTexture(), {width = 10, height = 10})})
            require('haylen.scene').push({
                update = function(self, dt) tree:update(dt) end,
                render = function(self)
                    graphics2d.beginWorld(camera)
                    box:draw()
                    if cover then
                        cover:draw()
                    end
                    tree:draw()
                end,
            })
            debug.setInspectorVisible(true)
            debug.setPicking(true)
        )");
        // clang-format on
        frames(2);
    }

    // The point of the visible area, which the fingers of the fixture take, where the camera shows a point of the world.
    [[nodiscard]] haylen::math::Vec2 screenOf(haylen::math::Vec2 world) {
        std::istringstream reply(getFixture().lua("local x, y = camera:worldToScreen(" + std::to_string(world.x) + ", " + std::to_string(world.y) + ") return x .. ' ' .. y"));
        haylen::math::Vec2 point{};
        reply >> point.x >> point.y;
        return point - getEngine().getViewport().getVisibleRect().getMin();
    }

    // Turns a point of the UI into the point of the visible area, and back.
    [[nodiscard]] haylen::math::Vec2 fromUi(haylen::math::Vec2 point) {
        return getUi().getBackend().toDesign(point) - getEngine().getViewport().getVisibleRect().getMin();
    }
    [[nodiscard]] haylen::math::Vec2 toUi(haylen::math::Vec2 point) {
        return getUi().getBackend().toUi(point + getEngine().getViewport().getVisibleRect().getMin());
    }

    void tap(haylen::math::Vec2 point) {
        touch(haylen::platform::Event::Type::TouchBegan, 1, point);
        frames();
        touch(haylen::platform::Event::Type::TouchEnded, 1, point);
        frames(2);
    }

    void slide(haylen::math::Vec2 from, haylen::math::Vec2 to) {
        touch(haylen::platform::Event::Type::TouchBegan, 1, from);
        frames();
        for (int step = 1; step <= 8; ++step) {
            touch(haylen::platform::Event::Type::TouchMoved, 1, from + (to - from) * (static_cast<float>(step) / 8.0F));
            frames();
        }

        touch(haylen::platform::Event::Type::TouchEnded, 1, to);
        frames(2);
    }

    [[nodiscard]] double number(const std::string& expression) {
        return std::stod(getFixture().lua("return " + expression));
    }
};

// A tap selects the sprite under it, which the root `selected` of the inspector shows, and the dot at its position moves it by the distance of the finger in the world, half of it through a camera that zooms by 2.
TEST_F(PickingTest, SelectsASpriteAndMovesItWithTheFinger) {
    tap(screenOf({40.0F, 20.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected() == box)"), "true");
    EXPECT_NE(getFixture().lua("for _, entry in ipairs(debug.inspect('selected')) do if entry.key == 'x' then return entry.value end end"), "");

    const haylen::math::Vec2 origin = screenOf({40.0F, 20.0F});
    slide(origin, origin + haylen::math::Vec2{30.0F, 20.0F});
    EXPECT_NEAR(number("box.x"), 55.0, 0.01);
    EXPECT_NEAR(number("box.y"), 30.0, 0.01);
}

// The square on the corner of the outline scales the sprite along its own axes, and the ring past the outline turns it around its position.
TEST_F(PickingTest, ScalesAndTurnsTheSelection) {
    tap(screenOf({40.0F, 20.0F}));
    ASSERT_EQ(getFixture().lua("return tostring(debug.selected() == box)"), "true");

    slide(screenOf({60.0F, 30.0F}), screenOf({80.0F, 30.0F}));
    EXPECT_NEAR(number("box.scaleX"), 2.0, 0.01);
    EXPECT_NEAR(number("box.scaleY"), 1.0, 0.01);

    // The ring lies along the first axis of the sprite, past the farthest corner of its outline, and a quarter turn of the finger around the position turns the sprite by a quarter turn.
    const haylen::math::Vec2 origin = toUi(screenOf({40.0F, 20.0F}));
    const float reach = (toUi(screenOf({80.0F, 30.0F})) - origin).getLength() + haylen::debug::SelectionOverlay::kTurnGap;
    slide(fromUi(origin + haylen::math::Vec2{reach, 0.0F}), fromUi(origin + haylen::math::Vec2{0.0F, reach}));
    EXPECT_NEAR(number("box.rotation"), std::numbers::pi / 2.0, 0.01);
}

// A tap on a node of a tree selects the node, whose position lives in the space of its parent, so the parent that scales by 2 halves what the finger moves it.
TEST_F(PickingTest, SelectsANodeAndMovesItInTheSpaceOfItsParent) {
    tap(screenOf({-40.0F, 30.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected() == child)"), "true");

    const haylen::math::Vec2 origin = screenOf({-40.0F, 30.0F});
    slide(origin, origin + haylen::math::Vec2{40.0F, 0.0F});
    EXPECT_NEAR(number("child.x"), 20.0, 0.01);
    EXPECT_NEAR(number("child.y"), 0.0, 0.01);
}

// The draw that draws over the others under the press wins, by its layer and then by its order, and a press where nothing draws clears the selection. A press on the handles of the selection takes the press instead of picking, so a pick at the point of a handle waits until the selection clears.
TEST_F(PickingTest, PicksWhatDrawsOnTopAndClearsOnEmptySpace) {
    getFixture().runLua("cover = graphics2d.newSprite(graphics.whiteTexture(), {x = 40, y = 20, width = 10, height = 10})");
    frames();
    tap(screenOf({40.0F, 20.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected() == cover)"), "true");

    getFixture().runLua("box.layer = 1");
    tap(screenOf({40.0F, 20.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected() == cover)"), "true");

    tap(screenOf({200.0F, -100.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected())"), "nil");

    tap(screenOf({40.0F, 20.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.selected() == box)"), "true");
}

// With the pick off, a press reaches the app and selects nothing.
TEST_F(PickingTest, LeavesThePressToTheAppWhileThePickIsOff) {
    getFixture().runLua("debug.setPicking(false)");
    tap(screenOf({40.0F, 20.0F}));
    EXPECT_EQ(getFixture().lua("return tostring(debug.picking()) .. ' ' .. tostring(debug.selected())"), "false nil");
    EXPECT_FALSE(getEngine().getRenderer2D().isPicking());
}

// Outside development nothing of the pick runs, and turning it on fails.
TEST(PickingOutsideDevelopmentTest, RefusesThePick) {
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Picking", "identifier": "com.haylen.picking", "debug": {"console": true}})"}});
    EXPECT_NE(fixture.lua("require('haylen.debug').setPicking(true)").find("The pick on the screen works only in development runs, such as the runs of \"haylen app run\"."), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').picking())"), "false");
    EXPECT_FALSE(fixture.engine().getRenderer2D().isPicking());
}

} // namespace haylen::lua::debug
