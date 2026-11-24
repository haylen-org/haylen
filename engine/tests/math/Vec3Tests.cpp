#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::math {

TEST(Vec3Test, ArithmeticAndComparison) {
    Vec3 value{3.0F, 4.0F, 12.0F};
    EXPECT_EQ(value + Vec3(1.0F, 2.0F, 3.0F), Vec3(4.0F, 6.0F, 15.0F));
    EXPECT_EQ(value - Vec3(1.0F, 2.0F, 3.0F), Vec3(2.0F, 2.0F, 9.0F));
    EXPECT_EQ(value * Vec3(2.0F, 0.5F, 1.0F), Vec3(6.0F, 2.0F, 12.0F));
    EXPECT_EQ(value / Vec3(3.0F, 2.0F, 4.0F), Vec3(1.0F, 2.0F, 3.0F));
    EXPECT_EQ(2.0F * value, Vec3(6.0F, 8.0F, 24.0F));
    EXPECT_EQ(value / 2.0F, Vec3(1.5F, 2.0F, 6.0F));
    EXPECT_EQ(-value, Vec3(-3.0F, -4.0F, -12.0F));
    EXPECT_FLOAT_EQ(value.getLength(), 13.0F);
    EXPECT_FLOAT_EQ(value.getLengthSquared(), 169.0F);

    value += Vec3(1.0F, 1.0F, 1.0F);
    value -= Vec3(2.0F, 2.0F, 2.0F);
    value *= Vec3(2.0F, 1.0F, 1.0F);
    value *= 2.0F;
    value /= 4.0F;
    EXPECT_EQ(value, Vec3(2.0F, 1.5F, 5.5F));
}

TEST(Vec3Test, FollowsTheRightHandedAxesWithYUpAndForwardAlongNegativeZ) {
    EXPECT_EQ(Vec3::cross(Vec3::right(), Vec3::up()), -Vec3::forward());
    EXPECT_EQ(Vec3::cross(Vec3::up(), -Vec3::forward()), Vec3::right());
    EXPECT_EQ(Vec3::forward(), Vec3(0.0F, 0.0F, -1.0F));
    EXPECT_FLOAT_EQ(Vec3::dot(Vec3::right(), Vec3::up()), 0.0F);
}

TEST(Vec3Test, CrossProductIsOrthogonalToBothSides) {
    const Vec3 lhs{0.3F, -2.0F, 5.5F};
    const Vec3 rhs{-4.0F, 1.25F, 0.75F};
    const Vec3 perpendicular = Vec3::cross(lhs, rhs);
    EXPECT_NEAR(Vec3::dot(perpendicular, lhs), 0.0F, 1e-4F);
    EXPECT_NEAR(Vec3::dot(perpendicular, rhs), 0.0F, 1e-4F);
    EXPECT_NEAR(Vec3::cross(lhs, lhs).getLength(), 0.0F, 1e-6F);
    EXPECT_EQ(Vec3::cross(rhs, lhs), -perpendicular);
}

TEST(Vec3Test, NormalizesStablyAcrossTheWholeFloatRange) {
    EXPECT_EQ(Vec3::up().getNormalized(), Vec3::up());
    EXPECT_TRUE(Vec3::approximately(Vec3(3.0F, 0.0F, 4.0F).getNormalized(), Vec3(0.6F, 0.0F, 0.8F)));
    EXPECT_TRUE(Vec3::approximately(Vec3(1e-30F, 0.0F, 1e-30F).getNormalized(), Vec3(std::sqrt(0.5F), 0.0F, std::sqrt(0.5F))));
    EXPECT_TRUE(Vec3::approximately(Vec3(3e37F, 4e37F, 0.0F).getNormalized(), Vec3(0.6F, 0.8F, 0.0F)));

    const float denormal = std::numeric_limits<float>::denorm_min();
    EXPECT_EQ(Vec3(0.0F, -denormal, 0.0F).getNormalized(), Vec3(0.0F, -1.0F, 0.0F));
}

TEST(Vec3Test, RefusesToNormalizeAVectorWithoutDirection) {
    EXPECT_THROW(
        {
            try {
                (void)Vec3{}.getNormalized();
            } catch (const std::invalid_argument& error) {
                EXPECT_STREQ(error.what(), "The vector has no direction, because its length is zero.");
                throw;
            }
        },
        std::invalid_argument);
    EXPECT_THROW((void)Vec3(std::numeric_limits<float>::infinity(), 0.0F, 0.0F).getNormalized(), std::invalid_argument);
    EXPECT_THROW((void)Vec3(-std::numeric_limits<float>::infinity(), 1.0F, 0.0F).getNormalized(), std::invalid_argument);
    EXPECT_THROW((void)Vec3(std::numeric_limits<float>::quiet_NaN(), 1.0F, 0.0F).getNormalized(), std::invalid_argument);
    EXPECT_THROW((void)Vec3::project({1.0F, 2.0F, 3.0F}, {}), std::invalid_argument);
}

TEST(Vec3Test, ProjectsReflectsAndRefracts) {
    EXPECT_EQ(Vec3::project({3.0F, 4.0F, 5.0F}, {0.0F, 2.0F, 0.0F}), Vec3(0.0F, 4.0F, 0.0F));
    EXPECT_EQ(Vec3::projectOnPlane({3.0F, 4.0F, 5.0F}, Vec3::up()), Vec3(3.0F, 0.0F, 5.0F));
    EXPECT_EQ(Vec3::reflect({1.0F, -1.0F, 0.0F}, Vec3::up()), Vec3(1.0F, 1.0F, 0.0F));

    const Vec3 incoming = Vec3(1.0F, -1.0F, 0.0F).getNormalized();
    const std::optional<Vec3> straight = Vec3::refract(incoming, Vec3::up(), 1.0F);
    ASSERT_TRUE(straight.has_value());
    EXPECT_TRUE(Vec3::approximately(*straight, incoming));

    const std::optional<Vec3> intoWater = Vec3::refract(incoming, Vec3::up(), 1.0F / 1.33F);
    ASSERT_TRUE(intoWater.has_value());
    EXPECT_NEAR(intoWater->getLength(), 1.0F, 1e-5F);
    EXPECT_NEAR(intoWater->x, std::sqrt(0.5F) / 1.33F, 1e-5F);
    EXPECT_FALSE(Vec3::refract(incoming, Vec3::up(), 1.5F).has_value());
}

TEST(Vec3Test, InterpolatesAndComparesWithTolerance) {
    EXPECT_EQ(Vec3::lerp({0.0F, 10.0F, -4.0F}, {10.0F, 20.0F, 4.0F}, 0.5F), Vec3(5.0F, 15.0F, 0.0F));
    EXPECT_EQ(Vec3::lerp({0.1F, 0.2F, 0.3F}, {7.0F, 8.0F, 9.0F}, 1.0F), Vec3(7.0F, 8.0F, 9.0F));
    EXPECT_EQ(Vec3::min({1.0F, 5.0F, -2.0F}, {2.0F, 3.0F, -3.0F}), Vec3(1.0F, 3.0F, -3.0F));
    EXPECT_EQ(Vec3::max({1.0F, 5.0F, -2.0F}, {2.0F, 3.0F, -3.0F}), Vec3(2.0F, 5.0F, -2.0F));
    EXPECT_FLOAT_EQ(Vec3::distance({1.0F, 2.0F, 3.0F}, {4.0F, 6.0F, 3.0F}), 5.0F);
    EXPECT_TRUE(Vec3::approximately({1.0F, 2.0F, 3.0F}, {1.000001F, 2.0F, 3.0F}));
    EXPECT_FALSE(Vec3::approximately({1.0F, 2.0F, 3.0F}, {1.1F, 2.0F, 3.0F}));
    EXPECT_FALSE(Vec3::approximately({std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F}, {std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F}));
    EXPECT_TRUE(Vec3{}.isZero());
    EXPECT_FALSE(Vec3(0.0F, 0.0F, std::numeric_limits<float>::infinity()).isFinite());
}

TEST(Vec4Test, ArithmeticNormalizationAndHomogeneousParts) {
    const Vec4 point = Vec4::fromVec3({1.0F, 2.0F, 3.0F}, 1.0F);
    EXPECT_EQ(point, Vec4(1.0F, 2.0F, 3.0F, 1.0F));
    EXPECT_EQ(point.getXyz(), Vec3(1.0F, 2.0F, 3.0F));
    EXPECT_EQ(point + point, point * 2.0F);
    EXPECT_EQ(point - point, Vec4{});
    EXPECT_EQ(-point / 2.0F, Vec4(-0.5F, -1.0F, -1.5F, -0.5F));
    EXPECT_FLOAT_EQ(Vec4::dot(point, {1.0F, 1.0F, 1.0F, 1.0F}), 7.0F);
    EXPECT_FLOAT_EQ(Vec4(1.0F, 1.0F, 1.0F, 1.0F).getLength(), 2.0F);
    EXPECT_EQ(Vec4(0.0F, 0.0F, 0.0F, 5.0F).getNormalized(), Vec4(0.0F, 0.0F, 0.0F, 1.0F));
    EXPECT_EQ(Vec4::lerp({}, {2.0F, 4.0F, 6.0F, 8.0F}, 0.5F), Vec4(1.0F, 2.0F, 3.0F, 4.0F));
    EXPECT_EQ(Vec4::min({1.0F, 5.0F, 0.0F, 9.0F}, {2.0F, 3.0F, 1.0F, -9.0F}), Vec4(1.0F, 3.0F, 0.0F, -9.0F));
    EXPECT_EQ(Vec4::max({1.0F, 5.0F, 0.0F, 9.0F}, {2.0F, 3.0F, 1.0F, -9.0F}), Vec4(2.0F, 5.0F, 1.0F, 9.0F));
    EXPECT_FLOAT_EQ(Vec4::distance({}, {1.0F, 1.0F, 1.0F, 1.0F}), 2.0F);
    EXPECT_TRUE(Vec4::approximately({1.0F, 2.0F, 3.0F, 4.0F}, {1.0F, 2.0F, 3.0F, 4.000001F}));
    EXPECT_THROW((void)Vec4{}.getNormalized(), std::invalid_argument);
    EXPECT_THROW((void)Vec4(0.0F, 0.0F, 0.0F, std::numeric_limits<float>::quiet_NaN()).getNormalized(), std::invalid_argument);
}

} // namespace haylen::math
