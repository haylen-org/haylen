#include <gtest/gtest.h>

#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/WindowPlacement.hpp"
#include "support/DisplayLayout.hpp"

namespace haylen::platform {

TEST(WindowPlacementTest, AnchorsWindowsToTheAreaOfADisplay) {
    EXPECT_EQ(test::DisplayLayout::place(R"("center")"), (math::Rect{760.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottom"})"), (math::Rect{760.0F, 940.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottom", "area": "full"})"), (math::Rect{760.0F, 980.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "top"})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "left"})"), (math::Rect{0.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "right"})"), (math::Rect{1520.0F, 470.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "topLeft", "offset": [16, 24]})"), (math::Rect{16.0F, 24.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "topRight"})"), (math::Rect{1520.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottomLeft"})"), (math::Rect{0.0F, 940.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottomRight", "offset": [-10, -10]})"), (math::Rect{1510.0F, 930.0F, 400.0F, 100.0F}));

    // Displays count from 1 or go by their ids, and a display past the last one or one that is not connected is the primary display.
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottom", "display": 2})"), (math::Rect{-920.0F, 1000.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottom", "display": "side-id"})"), (math::Rect{-920.0F, 1000.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "top", "display": 1})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "top", "display": 3})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "top", "display": "gone"})"), (math::Rect{760.0F, 0.0F, 400.0F, 100.0F}));

    // Without a primary display, the first display takes its place.
    std::vector<Display> unflagged = test::DisplayLayout::twoDisplays();
    unflagged.front().primary = false;
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "topLeft"})", unflagged), (math::Rect{0.0F, 0.0F, 400.0F, 100.0F}));
}

TEST(WindowPlacementTest, FillsAreasAndPlacesPoints) {
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "bottom", "fill": "width"})"), (math::Rect{0.0F, 940.0F, 1920.0F, 100.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"anchor": "right", "fill": "height"})"), (math::Rect{1520.0F, 0.0F, 400.0F, 1040.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"fill": "both", "area": "full", "display": 2})"), (math::Rect{-1440.0F, 200.0F, 1440.0F, 900.0F}));
    EXPECT_EQ(test::DisplayLayout::place(R"({"x": -300.5, "y": 250})"), (math::Rect{-300.5F, 250.0F, 400.0F, 100.0F}));

    // Anchored windows land on whole points, so their edges stay sharp.
    EXPECT_EQ(test::DisplayLayout::place(R"("center")", test::DisplayLayout::twoDisplays(), {401.0F, 101.0F}), (math::Rect{760.0F, 470.0F, 401.0F, 101.0F}));

    for (const char* position : {R"("center")", R"({"x": 12, "y": 34})", R"({"anchor": "bottomRight", "area": "full", "display": 2, "offset": [-4, 6], "fill": "height"})", R"({"anchor": "left", "display": "side-id"})"}) {
        const WindowPlacement placement = WindowPlacement::fromJson(core::Json::parse(position));
        EXPECT_EQ(WindowPlacement::fromJson(placement.toJson()), placement) << position;
    }
}

} // namespace haylen::platform
