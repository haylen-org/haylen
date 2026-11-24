#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "haylen/platform/Fold.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

TEST(FoldLuaTest, ReportsTheFoldOfTheDeviceInDesignUnits) {
    test::LuaFixture fixture({{"haylen.json", R"({"design": {"width": 960, "height": 540}})"}});
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return tostring(window.fold()) .. ' ' .. window.posture() .. ' ' .. #window.segments() .. ' ' .. tostring(window.segments()[1])"), "nil flat 1 Rect(0.0, 0.0, 960.0, 540.0)");

    fixture.host().setFold(haylen::platform::Fold{.bounds = {960.0F, 0.0F, 0.0F, 1080.0F}, .axis = haylen::platform::Fold::Axis::Vertical, .state = haylen::platform::Fold::State::HalfOpened, .separating = true});
    EXPECT_EQ(fixture.lua("local fold = window.fold() return tostring(fold.bounds) .. ' ' .. fold.axis .. ' ' .. fold.state .. ' ' .. tostring(fold.separating) .. ' ' .. tostring(fold.occluding)"), "Rect(480.0, 0.0, 0.0, 540.0) vertical halfOpened true false");
    EXPECT_EQ(fixture.lua("local left, right = table.unpack(window.segments()) return window.posture() .. ' ' .. tostring(left) .. ' ' .. tostring(right)"), "book Rect(0.0, 0.0, 480.0, 540.0) Rect(480.0, 0.0, 480.0, 540.0)");
}

TEST(FoldLuaTest, PublishesEveryChangeOfTheFoldOnce) {
    test::LuaFixture fixture;
    fixture.runLua("changes = {} require('haylen.events').on('windowFoldChanged', function(change) changes[#changes + 1] = change end)");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return #changes"), "0");

    fixture.host().setFold(haylen::platform::Fold{.bounds = {0.0F, 520.0F, 1920.0F, 40.0F}, .axis = haylen::platform::Fold::Axis::Horizontal, .state = haylen::platform::Fold::State::HalfOpened, .separating = true, .occluding = true});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("local change = changes[1] return #changes .. ' ' .. change.posture .. ' ' .. change.fold.y .. ' ' .. change.fold.height .. ' ' .. change.fold.axis .. ' ' .. tostring(change.fold.occluding) .. ' ' .. #change.segments .. ' ' .. change.segments[2].y"), "1 tabletop 520.0 40.0 horizontal true 2 560.0");

    fixture.host().setFold(std::nullopt);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local change = changes[2] return #changes .. ' ' .. change.posture .. ' ' .. tostring(change.fold) .. ' ' .. #change.segments .. ' ' .. change.segments[1].width"), "2 flat nil 1 1920.0");
}

TEST(FoldLuaTest, SimulatesAFoldOverTheOneOfTheDevice) {
    test::LuaFixture fixture({{"haylen.json", R"({"debug": {"fold": "tabletop"}})"}});
    fixture.runLua("window = require('haylen.window') changes = {} require('haylen.events').on('windowFoldChanged', function(change) changes[#changes + 1] = change.posture end)");
    EXPECT_EQ(fixture.lua("return window.foldSimulation() .. ' ' .. window.posture() .. ' ' .. tostring(window.fold().bounds)"), "tabletop tabletop Rect(0.0, 540.0, 1920.0, 0.0)");

    fixture.runLua("window.setFoldSimulation({axis = 'vertical', state = 'flat', hinge = 20})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local simulation = window.foldSimulation() return simulation.axis .. ' ' .. simulation.hinge .. ' ' .. window.posture() .. ' ' .. tostring(window.segments()[2]) .. ' ' .. changes[1]"), "vertical 20.0 flat Rect(970.0, 0.0, 950.0, 1080.0) flat");

    fixture.host().setFold(haylen::platform::Fold{.bounds = {960.0F, 0.0F, 0.0F, 1080.0F}, .state = haylen::platform::Fold::State::HalfOpened, .separating = true});
    fixture.runLua("window.setFoldSimulation(nil)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(window.foldSimulation()) .. ' ' .. window.posture() .. ' ' .. changes[2]"), "nil book book");
    EXPECT_NE(fixture.lua("window.setFoldSimulation('laptop')").find("There is no simulated fold named \"laptop\"."), std::string::npos);
}

} // namespace haylen::lua::platform
