#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "ui/components/collections/GridLayout.hpp"

namespace haylen::ui {

TEST(GridLayoutTest, PlacesItemsWithoutSpansByDividingTheirIndex) {
    GridLayout grid;
    grid.arrange({.count = 10, .crossLength = 340.0F, .gap = 20.0F, .lanes = 3}, 0);
    EXPECT_EQ(grid.getLineCount(), 4U);
    EXPECT_EQ(grid.getLine(7), 2U);
    EXPECT_EQ(grid.getFirstIndex(2), 6U);
    EXPECT_EQ(grid.getFirstIndex(4), 10U);
    const CollectionLayout::Slot slot = grid.getSlot(8);
    EXPECT_FLOAT_EQ(slot.crossStart, 240.0F);
    EXPECT_FLOAT_EQ(slot.crossLength, 100.0F);
}

TEST(GridLayoutTest, PlacesSpansAndFullLines) {
    GridLayout grid;
    const std::vector<std::uint16_t> spans{1, 2, 0, 1, 1, 1, 1};
    grid.arrange({.count = spans.size(), .crossLength = 316.0F, .gap = 8.0F, .lanes = 3, .spans = spans}, 0);

    ASSERT_EQ(grid.getLineCount(), 4U);
    EXPECT_EQ(grid.getFirstIndex(0), 0U);
    EXPECT_EQ(grid.getFirstIndex(1), 2U);
    EXPECT_EQ(grid.getFirstIndex(2), 3U);
    EXPECT_EQ(grid.getFirstIndex(3), 6U);
    EXPECT_EQ(grid.getLine(5), 2U);

    // Lanes are 100 units wide, so the second item starts one lane and one gap in and spans two lanes and the gap between them.
    const CollectionLayout::Slot second = grid.getSlot(1);
    EXPECT_FLOAT_EQ(second.crossStart, 108.0F);
    EXPECT_FLOAT_EQ(second.crossLength, 208.0F);
    const CollectionLayout::Slot full = grid.getSlot(2);
    EXPECT_FLOAT_EQ(full.crossStart, 0.0F);
    EXPECT_FLOAT_EQ(full.crossLength, 316.0F);
}

TEST(GridLayoutTest, FitsLanesToTheMinimumCellSize) {
    EXPECT_EQ(GridLayout::fitLanes(1000.0F, 320.0F, 16.0F), 3U);
    EXPECT_EQ(GridLayout::fitLanes(960.0F, 320.0F, 16.0F), 2U);
    EXPECT_EQ(GridLayout::fitLanes(100.0F, 320.0F, 16.0F), 1U);
}

TEST(GridLayoutTest, ArrangesAgainFromTheFirstChangedItem) {
    std::vector<std::uint16_t> spans(200, 1);
    for (std::size_t index = 0; index < spans.size(); index += 9) {
        spans[index] = 0;
    }
    GridLayout grid;
    grid.arrange({.count = spans.size(), .crossLength = 400.0F, .gap = 0.0F, .lanes = 4, .spans = spans}, 0);
    std::vector<std::size_t> starts;
    for (std::size_t line = 0; line < grid.getLineCount(); ++line) {
        starts.push_back(grid.getFirstIndex(line));
    }

    spans[50] = 3;
    grid.arrange({.count = spans.size(), .crossLength = 400.0F, .gap = 0.0F, .lanes = 4, .spans = spans}, 50);
    const std::size_t changedLine = grid.getLine(50);
    for (std::size_t line = 0; line < changedLine; ++line) {
        EXPECT_EQ(grid.getFirstIndex(line), starts[line]);
    }

    // Arranging from the first changed item gives the same lines as arranging everything.
    GridLayout fresh;
    fresh.arrange({.count = spans.size(), .crossLength = 400.0F, .gap = 0.0F, .lanes = 4, .spans = spans}, 0);
    ASSERT_EQ(grid.getLineCount(), fresh.getLineCount());
    for (std::size_t line = 0; line < fresh.getLineCount(); ++line) {
        EXPECT_EQ(grid.getFirstIndex(line), fresh.getFirstIndex(line));
    }
}

} // namespace haylen::ui
