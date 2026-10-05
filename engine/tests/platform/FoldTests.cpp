#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/FoldSimulation.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(FoldTest, SplitsTheWindowAtASeparatingFold) {
    const math::Vec2 size{2000.0F, 1000.0F};
    const Fold hinge{.bounds = {980.0F, 0.0F, 40.0F, 1000.0F}, .axis = Fold::Axis::Vertical, .state = Fold::State::Flat, .separating = true, .occluding = true};
    EXPECT_EQ(hinge.getSegments(size), (std::vector<math::Rect>{{0.0F, 0.0F, 980.0F, 1000.0F}, {1020.0F, 0.0F, 980.0F, 1000.0F}}));
    EXPECT_EQ(hinge.getPosture(), Fold::Posture::Flat);

    const Fold tabletop{.bounds = {0.0F, 500.0F, 2000.0F, 0.0F}, .axis = Fold::Axis::Horizontal, .state = Fold::State::HalfOpened, .separating = true};
    EXPECT_EQ(tabletop.getSegments(size), (std::vector<math::Rect>{{0.0F, 0.0F, 2000.0F, 500.0F}, {0.0F, 500.0F, 2000.0F, 500.0F}}));
    EXPECT_EQ(tabletop.getPosture(), Fold::Posture::Tabletop);

    // A seamless fold opened flat leaves the window whole, and a fold that the window only partly covers keeps its segments inside the window.
    const Fold flat{.bounds = {1000.0F, 0.0F, 0.0F, 1000.0F}};
    EXPECT_EQ(flat.getSegments(size), (std::vector<math::Rect>{{0.0F, 0.0F, 2000.0F, 1000.0F}}));
    const Fold book{.bounds = {1900.0F, 0.0F, 300.0F, 1000.0F}, .state = Fold::State::HalfOpened, .separating = true};
    EXPECT_EQ(book.getPosture(), Fold::Posture::Book);
    EXPECT_EQ(book.getSegments(size), (std::vector<math::Rect>{{0.0F, 0.0F, 1900.0F, 1000.0F}, {2000.0F, 0.0F, 0.0F, 1000.0F}}));
}

TEST(FoldSimulationTest, PlacesTheFoldOfAPresetOrAnObjectInTheMiddleOfTheWindow) {
    const FoldSimulation book = FoldSimulation::fromJson("book");
    const Fold folded = book.getFold({2000.0F, 1000.0F}, 2.0F);
    EXPECT_EQ(folded, (Fold{.bounds = {1000.0F, 0.0F, 0.0F, 1000.0F}, .axis = Fold::Axis::Vertical, .state = Fold::State::HalfOpened, .separating = true, .occluding = false}));
    EXPECT_EQ(book.toJson(), core::Json("book"));

    // A hinge is window points wide, hides what is under it and separates the window even when it lies flat.
    const Fold dual = FoldSimulation::fromJson("dualScreen").getFold({2000.0F, 1000.0F}, 2.0F);
    EXPECT_EQ(dual.bounds, (math::Rect{966.0F, 0.0F, 68.0F, 1000.0F}));
    EXPECT_TRUE(dual.separating);
    EXPECT_TRUE(dual.occluding);

    const FoldSimulation custom = FoldSimulation::fromJson(core::Json::parse(R"({"axis": "horizontal", "state": "flat", "hinge": 10})"));
    EXPECT_EQ(custom.getFold({800.0F, 600.0F}, 1.0F).bounds, (math::Rect{0.0F, 295.0F, 800.0F, 10.0F}));
    EXPECT_EQ(FoldSimulation::fromJson(custom.toJson()), custom);
    EXPECT_FALSE(FoldSimulation::fromJson(core::Json::parse(R"({"state": "flat"})")).getFold({800.0F, 600.0F}, 1.0F).separating);

    EXPECT_THROW((void)FoldSimulation::fromJson("laptop"), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(core::Json::parse(R"({"axis": "diagonal"})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(core::Json::parse(R"({"hinge": -1})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(core::Json::parse(R"({"width": 4})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(core::Json(3)), std::invalid_argument);
}

TEST(FoldLuaTest, ReportsTheFoldOfTheDeviceInDesignUnits) {
    test::EngineFixture fixture({{"app.json", R"({"design": {"width": 960, "height": 540}})"}});
    fixture.runLua("window = require('haylen.window')");
    EXPECT_EQ(fixture.lua("return tostring(window.fold()) .. ' ' .. window.posture() .. ' ' .. #window.segments() .. ' ' .. tostring(window.segments()[1])"), "nil flat 1 Rect(0.0, 0.0, 960.0, 540.0)");

    fixture.host().setFold(Fold{.bounds = {960.0F, 0.0F, 0.0F, 1080.0F}, .axis = Fold::Axis::Vertical, .state = Fold::State::HalfOpened, .separating = true});
    EXPECT_EQ(fixture.lua("local fold = window.fold() return tostring(fold.bounds) .. ' ' .. fold.axis .. ' ' .. fold.state .. ' ' .. tostring(fold.separating) .. ' ' .. tostring(fold.occluding)"), "Rect(480.0, 0.0, 0.0, 540.0) vertical halfOpened true false");
    EXPECT_EQ(fixture.lua("local left, right = table.unpack(window.segments()) return window.posture() .. ' ' .. tostring(left) .. ' ' .. tostring(right)"), "book Rect(0.0, 0.0, 480.0, 540.0) Rect(480.0, 0.0, 480.0, 540.0)");
}

TEST(FoldLuaTest, PublishesEveryChangeOfTheFoldOnce) {
    test::EngineFixture fixture;
    fixture.runLua("changes = {} require('haylen.events').on('windowFoldChanged', function(change) changes[#changes + 1] = change end)");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return #changes"), "0");

    fixture.host().setFold(Fold{.bounds = {0.0F, 520.0F, 1920.0F, 40.0F}, .axis = Fold::Axis::Horizontal, .state = Fold::State::HalfOpened, .separating = true, .occluding = true});
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("local change = changes[1] return #changes .. ' ' .. change.posture .. ' ' .. change.fold.y .. ' ' .. change.fold.height .. ' ' .. change.fold.axis .. ' ' .. tostring(change.fold.occluding) .. ' ' .. #change.segments .. ' ' .. change.segments[2].y"), "1 tabletop 520.0 40.0 horizontal true 2 560.0");

    fixture.host().setFold(std::nullopt);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local change = changes[2] return #changes .. ' ' .. change.posture .. ' ' .. tostring(change.fold) .. ' ' .. #change.segments .. ' ' .. change.segments[1].width"), "2 flat nil 1 1920.0");
}

TEST(FoldLuaTest, SimulatesAFoldOverTheOneOfTheDevice) {
    test::EngineFixture fixture({{"app.json", R"({"debug": {"fold": "tabletop"}})"}});
    fixture.runLua("window = require('haylen.window') changes = {} require('haylen.events').on('windowFoldChanged', function(change) changes[#changes + 1] = change.posture end)");
    EXPECT_EQ(fixture.lua("return window.foldSimulation() .. ' ' .. window.posture() .. ' ' .. tostring(window.fold().bounds)"), "tabletop tabletop Rect(0.0, 540.0, 1920.0, 0.0)");

    fixture.runLua("window.setFoldSimulation({axis = 'vertical', state = 'flat', hinge = 20})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local simulation = window.foldSimulation() return simulation.axis .. ' ' .. simulation.hinge .. ' ' .. window.posture() .. ' ' .. tostring(window.segments()[2]) .. ' ' .. changes[1]"), "vertical 20.0 flat Rect(970.0, 0.0, 950.0, 1080.0) flat");

    fixture.host().setFold(Fold{.bounds = {960.0F, 0.0F, 0.0F, 1080.0F}, .state = Fold::State::HalfOpened, .separating = true});
    fixture.runLua("window.setFoldSimulation(nil)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(window.foldSimulation()) .. ' ' .. window.posture() .. ' ' .. changes[2]"), "nil book book");
    EXPECT_NE(fixture.lua("window.setFoldSimulation('laptop')").find("There is no simulated fold named \"laptop\"."), std::string::npos);
}

TEST(FoldSimulationTest, RejectsAnInvalidFoldInAppJson) {
    try {
        (void)core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"fold": {"axis": "diagonal"}}})"));
        FAIL() << "An invalid fold must fail to load.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The \"debug.fold\" in \"app.json\" is invalid. The axis of a simulated fold is \"vertical\" or \"horizontal\", not \"diagonal\".");
    }
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"debug": {"fold": "book"}})")).toJson()["debug"]["fold"], core::Json("book"));
}

} // namespace haylen::platform
