#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "platform/NativeViews.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

class SafeAreaTest : public ::testing::Test {
  protected:
    static std::string app(std::string_view scaling) {
        return R"({"name": "Test App", "identifier": "com.haylen.tests", "design": {"width": 960, "height": 540, "scaling": ")" + std::string(scaling) + R"("}})";
    }

    static void expectSameRect(const haylen::math::Rect& actual, const haylen::math::Rect& expected) {
        EXPECT_NEAR(actual.x, expected.x, 0.01F);
        EXPECT_NEAR(actual.y, expected.y, 0.01F);
        EXPECT_NEAR(actual.width, expected.width, 0.01F);
        EXPECT_NEAR(actual.height, expected.height, 0.01F);
    }
};

TEST_F(SafeAreaTest, KeepsTheWholeWindowSafeWhereNothingCoversIt) {
    // A window without a cutout, rounded corners or bars of the system over it has no device insets, so every scaling keeps its whole visible area safe, letterboxed or not.
    for (const std::string_view scaling : {"fit", "fill", "stretch", "expand", "pixelPerfect"}) {
        test::LuaFixture fixture({{"haylen.json", app(scaling)}});
        for (const haylen::math::Vec2 size : {haylen::math::Vec2{1920.0F, 1080.0F}, haylen::math::Vec2{2400.0F, 1080.0F}, haylen::math::Vec2{1080.0F, 1920.0F}}) {
            fixture.host().resize(size);
            fixture.frames(1);
            expectSameRect(fixture.engine().getViewport().getSafeRect(), fixture.engine().getViewport().getVisibleRect());
        }
    }

    // Insets of the device that fall in the bars of a letterboxed design leave the design whole, and a reservation takes only its own edge.
    test::LuaFixture fixture({{"haylen.json", app("fit")}});
    fixture.host().resize({2400.0F, 1080.0F});
    fixture.host().setSafeAreaInsets({.left = 120.0F, .right = 200.0F});
    fixture.host().getNativeViews().reserveInsets("banner", {.bottom = 108.0F});
    fixture.frames(1);
    expectSameRect(fixture.engine().getViewport().getSafeRect(), {0.0F, 0.0F, 960.0F, 486.0F});

    // An inset that reaches past the bars covers the design from there.
    fixture.host().setSafeAreaInsets({.left = 300.0F});
    fixture.frames(1);
    expectSameRect(fixture.engine().getViewport().getSafeRect(), {30.0F, 0.0F, 930.0F, 486.0F});
}

TEST_F(SafeAreaTest, ReportsTheCutoutsOfTheScreenInDesignUnits) {
    // The cutouts of the device reach Lua in design units, and the event announces each change with the new list.
    test::LuaFixture fixture({{"haylen.json", app("fit")}});
    fixture.host().resize({1920.0F, 1080.0F});
    fixture.frames(1);
    fixture.runLua("heard = {} require('haylen.events').on('windowCutoutsChanged', function(info) heard[#heard + 1] = #info.cutouts end)");
    fixture.host().setCutouts({{900.0F, 0.0F, 120.0F, 60.0F}});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local cutout = require('haylen.viewport').cutouts()[1] return table.concat({cutout.x, cutout.y, cutout.width, cutout.height}, ',')"), "450.0,0.0,60.0,30.0");
    fixture.host().setCutouts({});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return #require('haylen.viewport').cutouts() .. ' ' .. table.concat(heard, ',')"), "0 1,0");

    // A simulated device brings the cutout of its screen, which turns with the window.
    fixture.runLua("require('haylen.viewport').setSafeAreaSimulation('iphoneDynamicIsland')");
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    ASSERT_EQ(fixture.engine().getCutouts().size(), 1U);
    expectSameRect(fixture.engine().getCutouts()[0], {1080.0F * 133.5F / 393.0F, 1920.0F * 11.0F / 852.0F, 1080.0F * 126.0F / 393.0F, 1920.0F * 37.0F / 852.0F});
    fixture.host().resize({1920.0F, 1080.0F});
    fixture.frames(1);
    ASSERT_EQ(fixture.engine().getCutouts().size(), 1U);
    expectSameRect(fixture.engine().getCutouts()[0], {1920.0F * 11.0F / 852.0F, 1080.0F * 133.5F / 393.0F, 1920.0F * 37.0F / 852.0F, 1080.0F * 126.0F / 393.0F});
}

} // namespace haylen::lua::platform
