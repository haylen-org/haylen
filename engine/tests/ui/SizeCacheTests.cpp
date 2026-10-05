#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "ui/components/collections/GridLayout.hpp"
#include "ui/components/collections/LinearLayout.hpp"
#include "ui/components/collections/SizeCache.hpp"

namespace haylen::ui {

namespace {

class SizeCacheTest : public ::testing::Test {
  protected:
    // Lays out items of one type with a declared estimate in a list and measures each one at its length.
    void measureAll(const std::vector<float>& lengths) {
        sizes.setEstimates(std::vector<float>{kEstimate}, kFallback);
        sizes.assign(std::vector<std::uint16_t>(lengths.size(), 0));
        arrange();
        for (std::size_t index = 0; index < lengths.size(); ++index) {
            sizes.measure(index, lengths[index]);
        }
    }

    void arrange(float gap = 0.0F) {
        layout.arrange({.count = sizes.size(), .crossLength = 100.0F, .gap = gap}, 0);
        sizes.arrange(layout, gap);
    }

    static constexpr float kEstimate = 50.0F;
    static constexpr float kFallback = 64.0F;

    LinearLayout layout;
    SizeCache sizes;
};

} // namespace

TEST_F(SizeCacheTest, FindsLinesByOffset) {
    std::mt19937 random(7);
    std::uniform_real_distribution<float> length(20.0F, 140.0F);
    std::vector<float> lengths(100000);
    for (float& value : lengths) {
        value = length(random);
    }
    measureAll(lengths);

    double expected = 0.0;
    for (std::size_t line = 0; line < lengths.size(); ++line) {
        ASSERT_NEAR(sizes.getLineOffset(line), expected, 0.01) << line;
        if (line % 97 == 0) {
            EXPECT_EQ(sizes.findLine(expected + 0.5), line);
        }
        expected += lengths[line];
    }
    EXPECT_NEAR(sizes.getTotal(), expected, 0.01);
    EXPECT_EQ(sizes.findLine(-10.0), 0U);
    EXPECT_EQ(sizes.findLine(expected + 1000.0), lengths.size() - 1);
}

TEST_F(SizeCacheTest, AddsTheGapBetweenLinesButNotAfterTheLast) {
    measureAll({10.0F, 20.0F, 30.0F});
    arrange(5.0F);
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getLineOffset(2)), 40.0F);
    EXPECT_FLOAT_EQ(sizes.getLineLength(2), 30.0F);
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 70.0F);
    EXPECT_EQ(sizes.findLine(33.0), 1U);
    EXPECT_EQ(sizes.findLine(36.0), 1U);
    EXPECT_EQ(sizes.findLine(41.0), 2U);
}

TEST_F(SizeCacheTest, KeepsOffsetsAfterInsertingAndErasingLines) {
    std::vector<float> lengths(1000);
    for (std::size_t index = 0; index < lengths.size(); ++index) {
        lengths[index] = 30.0F + static_cast<float>(index % 7) * 10.0F;
    }
    measureAll(lengths);

    // Inserted items take the estimate of their type, and the measured items keep their lengths wherever they moved.
    sizes.insert(500, std::vector<std::uint16_t>(10, 0));
    lengths.insert(lengths.begin() + 500, 10, kEstimate);
    sizes.erase(20, 3);
    lengths.erase(lengths.begin() + 20, lengths.begin() + 23);
    EXPECT_TRUE(sizes.needsArrange());
    arrange();

    double expected = 0.0;
    for (std::size_t line = 0; line < lengths.size(); ++line) {
        ASSERT_NEAR(sizes.getLineOffset(line), expected, 0.01) << line;
        expected += lengths[line];
    }
}

TEST_F(SizeCacheTest, KeepsMeasuredLengthsThroughAReorder) {
    measureAll({10.0F, 20.0F, 30.0F});
    sizes.remap(std::vector<std::size_t>{2, SizeCache::kNew, 0}, std::vector<std::uint16_t>{0, 0, 0});
    arrange();
    EXPECT_FLOAT_EQ(sizes.getItemLength(0), 30.0F);
    EXPECT_FLOAT_EQ(sizes.getItemLength(1), kEstimate);
    EXPECT_FALSE(sizes.isMeasured(1));
    EXPECT_FLOAT_EQ(sizes.getItemLength(2), 10.0F);
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 90.0F);
}

TEST_F(SizeCacheTest, EstimatesUnmeasuredItemsWithTheMeanOfTheirType) {
    // The first type learns its estimate, and the second declares one.
    sizes.setEstimates(std::vector<float>{0.0F, 120.0F}, kFallback);
    std::vector<std::uint16_t> types(12, 0);
    types[11] = 1;
    sizes.assign(types);
    arrange();
    EXPECT_FLOAT_EQ(sizes.getItemLength(10), kFallback);

    for (std::size_t index = 0; index < 10; ++index) {
        sizes.measure(index, 80.0F);
    }
    EXPECT_TRUE(sizes.needsArrange());
    arrange();
    EXPECT_FLOAT_EQ(sizes.getItemLength(10), 80.0F);
    EXPECT_FLOAT_EQ(sizes.getItemLength(11), 120.0F);
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 80.0F * 11.0F + 120.0F);
}

TEST_F(SizeCacheTest, KeepsStaleLengthsAsEstimatesUntilMeasuredAgain) {
    measureAll({10.0F, 20.0F});
    sizes.markStale();
    EXPECT_FALSE(sizes.isMeasured(0));
    EXPECT_FLOAT_EQ(sizes.getItemLength(0), 10.0F);
    sizes.measure(0, 15.0F);
    EXPECT_TRUE(sizes.isMeasured(0));
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 35.0F);
}

TEST_F(SizeCacheTest, MakesEveryLineOfAGridAsLongAsItsLongestItem) {
    GridLayout grid;
    sizes.setEstimates(std::vector<float>{kEstimate}, kFallback);
    sizes.assign(std::vector<std::uint16_t>(7, 0));
    grid.arrange({.count = 7, .crossLength = 300.0F, .gap = 0.0F, .lanes = 3}, 0);
    sizes.arrange(grid, 0.0F);
    sizes.measure(1, 90.0F);
    sizes.measure(3, 20.0F);

    EXPECT_EQ(sizes.getLineCount(), 3U);
    EXPECT_FLOAT_EQ(sizes.getLineLength(0), 90.0F);
    EXPECT_FLOAT_EQ(sizes.getLineLength(1), kEstimate);
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 90.0F + kEstimate * 2.0F);
}

TEST_F(SizeCacheTest, GivesFixedTypesTheirLengthWithoutMeasuring) {
    sizes.setEstimates(std::vector<float>{kEstimate}, kFallback);
    sizes.assign(std::vector<std::uint16_t>(4, 0));
    sizes.setFixedLength(0, 75.0F);
    arrange();
    EXPECT_TRUE(sizes.isFixed(2));
    EXPECT_TRUE(sizes.isMeasured(2));
    EXPECT_FLOAT_EQ(static_cast<float>(sizes.getTotal()), 300.0F);
}

} // namespace haylen::ui
