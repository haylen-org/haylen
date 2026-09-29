#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "haylen/math/Noise2D.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/ShuffleBag.hpp"
#include "haylen/math/Spring.hpp"
#include "haylen/math/WeightedChoice.hpp"

namespace haylen::math {

TEST(SpringTest, ReachesTheTargetWithoutOvershooting) {
    Spring spring(0.0F, 0.25F);
    float previous = 0.0F;
    for (int frame = 0; frame < 120; ++frame) {
        const float value = spring.update(10.0F, 1.0F / 60.0F);
        EXPECT_GE(value, previous);
        EXPECT_LE(value, 10.0F);
        previous = value;
    }
    EXPECT_NEAR(spring.getValue(), 10.0F, 0.01F);

    // Frame rate barely changes the path.
    Spring fast(0.0F, 0.25F);
    Spring slow(0.0F, 0.25F);
    for (int frame = 0; frame < 30; ++frame) {
        (void)fast.update(10.0F, 1.0F / 120.0F);
        (void)fast.update(10.0F, 1.0F / 120.0F);
        (void)slow.update(10.0F, 1.0F / 60.0F);
    }
    EXPECT_NEAR(fast.getValue(), slow.getValue(), 0.05F);

    spring.setSmoothTime(0.0F);
    EXPECT_GT(spring.getSmoothTime(), 0.0F);
    spring.setValue(3.0F);
    spring.setVelocity(1.0F);
    EXPECT_EQ(spring.update(3.0F, 0.0F), 3.0F);
    EXPECT_EQ(spring.getVelocity(), 1.0F);
}

TEST(SpringTest, DampsVectors) {
    Vec2 velocity{};
    Vec2 position{};
    for (int frame = 0; frame < 180; ++frame) {
        position = Spring::smoothDamp(position, {30.0F, -40.0F}, velocity, 0.3F, 1.0F / 60.0F);
    }
    EXPECT_NEAR(position.x, 30.0F, 0.01F);
    EXPECT_NEAR(position.y, -40.0F, 0.01F);

    // A huge step lands on the target and stops there.
    Vec2 jump{};
    EXPECT_NEAR(Spring::smoothDamp(Vec2{}, {5.0F, 0.0F}, jump, 0.1F, 100.0F).x, 5.0F, 1e-4F);
    EXPECT_LT(jump.getLength(), 1e-3F);
}

TEST(ShuffleBagTest, DealsEveryItemOncePerRound) {
    const std::array<std::uint32_t, 3> counts{1, 2, 3};
    ShuffleBag bag(counts);
    Random random(3);
    EXPECT_EQ(bag.size(), 6U);

    for (int round = 0; round < 5; ++round) {
        std::array<int, 3> seen{};
        for (int draw = 0; draw < 6; ++draw) {
            ++seen[bag.next(random)];
        }
        EXPECT_EQ(seen, (std::array<int, 3>{1, 2, 3}));
        EXPECT_EQ(bag.getRemaining(), 0U);
    }

    (void)bag.next(random);
    bag.refill();
    EXPECT_EQ(bag.getRemaining(), 6U);
    EXPECT_THROW(ShuffleBag(std::array<std::uint32_t, 2>{0, 0}), std::invalid_argument);
}

TEST(WeightedChoiceTest, PicksInProportionToTheWeights) {
    const std::array<float, 4> weights{1.0F, 0.0F, 3.0F, 6.0F};
    const WeightedChoice choice(weights);
    EXPECT_FLOAT_EQ(choice.getProbability(3), 0.6F);
    EXPECT_EQ(choice.getProbability(9), 0.0F);

    Random random(11);
    std::array<int, 4> counts{};
    for (int pick = 0; pick < 100000; ++pick) {
        ++counts[choice.pick(random)];
    }
    EXPECT_EQ(counts[1], 0);
    EXPECT_NEAR(counts[0] / 100000.0, 0.1, 0.01);
    EXPECT_NEAR(counts[2] / 100000.0, 0.3, 0.01);
    EXPECT_NEAR(counts[3] / 100000.0, 0.6, 0.01);

    EXPECT_THROW(WeightedChoice(std::array<float, 0>{}), std::invalid_argument);
    EXPECT_THROW(WeightedChoice(std::array<float, 2>{1.0F, -1.0F}), std::invalid_argument);
    EXPECT_THROW(WeightedChoice(std::array<float, 1>{0.0F}), std::invalid_argument);
}

TEST(NoiseTest, WorleyMeasuresTheDistanceToFeaturePoints) {
    const Noise2D noise(5);
    const Noise2D::Cellular same = noise.worley(3.25F, 7.5F);
    const Noise2D::Cellular again = Noise2D(5).worley(3.25F, 7.5F);
    EXPECT_EQ(same.nearest, again.nearest);
    EXPECT_EQ(same.cell, again.cell);

    float closest = 10.0F;
    for (int step = 0; step < 400; ++step) {
        const Noise2D::Cellular value = noise.worley(static_cast<float>(step % 20) * 0.37F, static_cast<float>(step / 20) * 0.41F);
        EXPECT_GE(value.nearest, 0.0F);
        EXPECT_LE(value.nearest, value.second);
        EXPECT_LT(value.nearest, 1.5F);
        EXPECT_GE(value.cell, 0.0F);
        EXPECT_LT(value.cell, 1.0F);
        closest = std::min(closest, value.nearest);
    }
    EXPECT_LT(closest, 0.2F);
}

TEST(NoiseTest, WarpMovesPointsWithinTheAmplitude) {
    const Noise2D noise(9);
    float moved = 0.0F;
    for (int step = 0; step < 100; ++step) {
        const Vec2 point{static_cast<float>(step) * 1.3F, static_cast<float>(step) * 0.7F};
        const Vec2 warped = noise.warp(point.x, point.y, 5.0F, 0.1F);
        EXPECT_LE(std::fabs(warped.x - point.x), 5.0F);
        EXPECT_LE(std::fabs(warped.y - point.y), 5.0F);
        moved += Vec2::distance(point, warped);
    }
    EXPECT_GT(moved, 10.0F);
    EXPECT_EQ(noise.warp(2.0F, 3.0F, 4.0F), Noise2D(9).warp(2.0F, 3.0F, 4.0F));
}

TEST(PoissonDiskTest, VariesTheSpacingWithADistanceMap) {
    Random random(21);
    PoissonDisk::Options options{.area = {0.0F, 0.0F, 400.0F, 200.0F}, .minimumDistance = 10.0F, .maximumDistance = 30.0F};
    // Points crowd on the left, where the map asks for the minimum distance, and spread out to the right.
    options.distance = [](Vec2 point) { return point.x < 200.0F ? 10.0F : 30.0F; };
    const std::vector<Vec2> points = PoissonDisk::sample(options, random);

    std::size_t left = 0;
    for (std::size_t first = 0; first < points.size(); ++first) {
        left += points[first].x < 200.0F ? 1U : 0U;
        for (std::size_t second = first + 1; second < points.size(); ++second) {
            const float limit = points[first].x < 200.0F && points[second].x < 200.0F ? 10.0F : 30.0F;
            EXPECT_GE(Vec2::distance(points[first], points[second]), limit - 1e-3F);
        }
    }
    EXPECT_GT(left, (points.size() - left) * 4);

    options.maximumDistance = 5.0F;
    EXPECT_THROW((void)PoissonDisk::sample(options, random), std::invalid_argument);
}

} // namespace haylen::math
