#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/math/HampelFilter.hpp"
#include "haylen/math/KalmanFilter.hpp"
#include "haylen/math/LowPassFilter.hpp"
#include "haylen/math/MedianFilter.hpp"
#include "haylen/math/MovingAverage.hpp"
#include "haylen/math/Random.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::math {

// The first update passes as it is, a step moves by the share its cutoff gives the time, and two half steps move as far as one whole step.
TEST(SignalFilterTest, LowPassMovesTowardTheValuesAtAnyFrameRate) {
    LowPassFilter filter(1.0F, 1);
    EXPECT_FLOAT_EQ(filter.update(1.0F / 60.0F, 0.0F), 0.0F);
    const float step = filter.update(1.0F / 60.0F, 10.0F);
    EXPECT_NEAR(step, 10.0F * (1.0F - std::exp(-2.0F * 3.14159265F / 60.0F)), 0.0001F);

    LowPassFilter halves(1.0F, 1);
    (void)halves.update(0.0F, 0.0F);
    (void)halves.update(1.0F / 120.0F, 10.0F);
    EXPECT_NEAR(halves.update(1.0F / 120.0F, 10.0F), step, 0.0001F);

    filter.reset();
    EXPECT_FLOAT_EQ(filter.update(1.0F, 4.0F), 4.0F);
    EXPECT_THROW(filter.setCutoff(0.0F), std::invalid_argument);
    EXPECT_THROW(LowPassFilter(1.0F, 5), std::invalid_argument);
}

TEST(SignalFilterTest, MovingAverageAveragesTheUpdatesOfItsWindow) {
    MovingAverage average(3, 1);
    std::vector<float> results;
    for (const float value : {1.0F, 2.0F, 3.0F, 4.0F, 8.0F}) {
        results.push_back(average.update(value));
    }
    EXPECT_EQ(results, (std::vector<float>{1.0F, 1.5F, 2.0F, 3.0F, 5.0F}));

    MovingAverage positions(2, 2);
    std::array<float, 2> first{0.0F, 10.0F};
    positions.update(first);
    std::array<float, 2> second{4.0F, 20.0F};
    positions.update(second);
    EXPECT_EQ(second, (std::array<float, 2>{2.0F, 15.0F}));
    EXPECT_THROW(MovingAverage(0, 1), std::invalid_argument);
}

// A median keeps steps sharp and drops a single spike, which an average would smear.
TEST(SignalFilterTest, MedianDropsSingleSpikesAndKeepsSteps) {
    MedianFilter filter(3, 1);
    std::vector<float> results;
    for (const float value : {1.0F, 1.0F, 90.0F, 1.0F, 5.0F, 5.0F, 5.0F}) {
        results.push_back(filter.update(value));
    }
    EXPECT_EQ(results, (std::vector<float>{1.0F, 1.0F, 1.0F, 1.0F, 5.0F, 5.0F, 5.0F}));
    std::array<float, 4> even{4.0F, 1.0F, 3.0F, 2.0F};
    EXPECT_FLOAT_EQ(MedianFilter::median(even), 2.5F);
}

// Every value that fits the spread of its window passes exactly, and only the spike takes the median.
TEST(SignalFilterTest, HampelReplacesOnlyOutliers) {
    HampelFilter filter(5, 3.0F, 1);
    for (const float value : {10.0F, 10.2F, 9.9F, 10.1F}) {
        EXPECT_FLOAT_EQ(filter.update(value), value);
        EXPECT_FALSE(filter.isOutlier());
    }
    EXPECT_FLOAT_EQ(filter.update(60.0F), 10.1F);
    EXPECT_TRUE(filter.isOutlier());
    EXPECT_FLOAT_EQ(filter.update(10.05F), 10.05F);
    EXPECT_FALSE(filter.isOutlier());
    EXPECT_THROW(HampelFilter(5, 0.0F, 1), std::invalid_argument);
}

// Noisy measurements of a steady value give estimates that stay much closer to it than the measurements, and the filter grows surer of them.
TEST(SignalFilterTest, KalmanEstimatesASteadyValueFromNoisyMeasurements) {
    KalmanFilter filter(0.001F, 4.0F, 1);
    Random random(7);
    float measuredError = 0.0F;
    float estimatedError = 0.0F;
    float uncertainty = 0.0F;
    for (int step = 0; step < 400; ++step) {
        const float measured = 5.0F + random.range(-3.0F, 3.0F);
        const float estimated = filter.update(measured);
        if (step == 10) {
            uncertainty = filter.getUncertainty();
        }
        if (step >= 100) {
            measuredError += std::abs(measured - 5.0F);
            estimatedError += std::abs(estimated - 5.0F);
        }
    }
    EXPECT_LT(estimatedError, measuredError * 0.25F);
    EXPECT_LT(filter.getUncertainty(), uncertainty);
    EXPECT_THROW(KalmanFilter(0.0F, 1.0F, 1), std::invalid_argument);
}

// The filters keep their windows from the start, so smoothing every frame never allocates.
TEST(SignalFilterTest, UpdatesWithoutAllocating) {
    MovingAverage average(32, 2);
    MedianFilter median(15, 2);
    HampelFilter hampel(15, 3.0F, 2);
    KalmanFilter kalman(0.01F, 1.0F, 2);
    LowPassFilter lowPass(4.0F, 2);
    const test::AllocationTracker tracker;
    for (int step = 0; step < 100; ++step) {
        std::array<float, 2> values{static_cast<float>(step), static_cast<float>(step % 7)};
        average.update(values);
        median.update(values);
        hampel.update(values);
        kalman.update(values);
        lowPass.update(1.0F / 60.0F, values);
    }
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST(SignalFilterLuaTest, SmoothsNumbersAndVectorsFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("m = require('haylen.math') average = m.movingAverage({window = 2, dimensions = 2})");
    EXPECT_EQ(fixture.lua("average:update(0, 10) local x, y = average:update(4, 20) return x .. ' ' .. y"), "2.0 15.0");
    EXPECT_EQ(fixture.lua("local point = average:update(m.vec2(8, 30)) return tostring(point.x) .. ' ' .. tostring(point.y)"), "6.0 25.0");
    EXPECT_EQ(fixture.lua("local median = m.medianFilter({window = 3}) median:update(1) median:update(90) return median:update(1)"), "1.0");
    EXPECT_EQ(fixture.lua("local hampel = m.hampelFilter() for _ = 1, 4 do hampel:update(10) end local value = hampel:update(60) return value .. ' ' .. tostring(hampel.outlier)"), "10.0 true");
    EXPECT_EQ(fixture.lua("local lowPass = m.lowPassFilter({cutoff = 2}) lowPass:update(0.1, 0) lowPass.cutoff = 3 return lowPass.cutoff .. ' ' .. lowPass.dimensions"), "3.0 1");
    EXPECT_EQ(fixture.lua("local kalman = m.kalmanFilter({processNoise = 0.1, measurementNoise = 2}) kalman:update(5) return tostring(kalman.uncertainty)"), "2.0");
    EXPECT_NE(fixture.lua("average:update(1)").find("The filter takes 2 values, not 1."), std::string::npos);
    EXPECT_NE(fixture.lua("m.medianFilter({windows = 3})").find("Unknown option \"windows\"."), std::string::npos);
    EXPECT_NE(fixture.lua("m.movingAverage({window = 0})").find("The window of a filter holds from 1 to 1024 updates, not 0."), std::string::npos);
}

} // namespace haylen::math
