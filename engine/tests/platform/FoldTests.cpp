#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/FoldSimulation.hpp"

namespace haylen::platform {

TEST(FoldTest, SplitsTheWindowAtASeparatingFold) {
    const haylen::math::Vec2 size{2000.0F, 1000.0F};
    const Fold hinge{.bounds = {980.0F, 0.0F, 40.0F, 1000.0F}, .axis = Fold::Axis::Vertical, .state = Fold::State::Flat, .separating = true, .occluding = true};
    EXPECT_EQ(hinge.getSegments(size), (std::vector<haylen::math::Rect>{{0.0F, 0.0F, 980.0F, 1000.0F}, {1020.0F, 0.0F, 980.0F, 1000.0F}}));
    EXPECT_EQ(hinge.getPosture(), Fold::Posture::Flat);

    const Fold tabletop{.bounds = {0.0F, 500.0F, 2000.0F, 0.0F}, .axis = Fold::Axis::Horizontal, .state = Fold::State::HalfOpened, .separating = true};
    EXPECT_EQ(tabletop.getSegments(size), (std::vector<haylen::math::Rect>{{0.0F, 0.0F, 2000.0F, 500.0F}, {0.0F, 500.0F, 2000.0F, 500.0F}}));
    EXPECT_EQ(tabletop.getPosture(), Fold::Posture::Tabletop);

    // A seamless fold opened flat leaves the window whole, and a fold that the window only partly covers keeps its segments inside the window.
    const Fold flat{.bounds = {1000.0F, 0.0F, 0.0F, 1000.0F}};
    EXPECT_EQ(flat.getSegments(size), (std::vector<haylen::math::Rect>{{0.0F, 0.0F, 2000.0F, 1000.0F}}));
    const Fold book{.bounds = {1900.0F, 0.0F, 300.0F, 1000.0F}, .state = Fold::State::HalfOpened, .separating = true};
    EXPECT_EQ(book.getPosture(), Fold::Posture::Book);
    EXPECT_EQ(book.getSegments(size), (std::vector<haylen::math::Rect>{{0.0F, 0.0F, 1900.0F, 1000.0F}, {2000.0F, 0.0F, 0.0F, 1000.0F}}));
}

TEST(FoldSimulationTest, PlacesTheFoldOfAPresetOrAnObjectInTheMiddleOfTheWindow) {
    const FoldSimulation book = FoldSimulation::fromJson("book");
    const Fold folded = book.getFold({2000.0F, 1000.0F}, 2.0F);
    EXPECT_EQ(folded, (Fold{.bounds = {1000.0F, 0.0F, 0.0F, 1000.0F}, .axis = Fold::Axis::Vertical, .state = Fold::State::HalfOpened, .separating = true, .occluding = false}));
    EXPECT_EQ(book.toJson(), haylen::core::Json("book"));

    // A hinge is window points wide, hides what is under it and separates the window even when it lies flat.
    const Fold dual = FoldSimulation::fromJson("dualScreen").getFold({2000.0F, 1000.0F}, 2.0F);
    EXPECT_EQ(dual.bounds, (haylen::math::Rect{966.0F, 0.0F, 68.0F, 1000.0F}));
    EXPECT_TRUE(dual.separating);
    EXPECT_TRUE(dual.occluding);

    const FoldSimulation custom = FoldSimulation::fromJson(haylen::core::Json::parse(R"({"axis": "horizontal", "state": "flat", "hinge": 10})"));
    EXPECT_EQ(custom.getFold({800.0F, 600.0F}, 1.0F).bounds, (haylen::math::Rect{0.0F, 295.0F, 800.0F, 10.0F}));
    EXPECT_EQ(FoldSimulation::fromJson(custom.toJson()), custom);
    EXPECT_FALSE(FoldSimulation::fromJson(haylen::core::Json::parse(R"({"state": "flat"})")).getFold({800.0F, 600.0F}, 1.0F).separating);

    EXPECT_THROW((void)FoldSimulation::fromJson("laptop"), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json::parse(R"({"axis": "diagonal"})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json::parse(R"({"axis": 1})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json::parse(R"({"state": true})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json::parse(R"({"hinge": -1})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json::parse(R"({"width": 4})")), std::invalid_argument);
    EXPECT_THROW((void)FoldSimulation::fromJson(haylen::core::Json(3)), std::invalid_argument);
}

TEST(FoldSimulationTest, RejectsAnInvalidFoldInHaylenJson) {
    try {
        (void)haylen::core::AppConfig::fromJson(haylen::core::Json::parse(R"({"debug": {"fold": {"axis": "diagonal"}}})"));
        FAIL() << "An invalid fold must fail to load.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The \"debug.fold\" in \"haylen.json\" is invalid. The axis of a simulated fold is \"vertical\" or \"horizontal\", not \"diagonal\".");
    }

    EXPECT_EQ(haylen::core::AppConfig::fromJson(haylen::core::Json::parse(R"({"debug": {"fold": "book"}})")).toJson()["debug"]["fold"], haylen::core::Json("book"));
}

} // namespace haylen::platform
