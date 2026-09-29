#include <gtest/gtest.h>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

TEST(Vec2Test, ArithmeticAndComparison) {
    Vec2 value{3.0F, 4.0F};
    EXPECT_EQ(value + Vec2(1.0F, 2.0F), Vec2(4.0F, 6.0F));
    EXPECT_EQ(value - Vec2(1.0F, 2.0F), Vec2(2.0F, 2.0F));
    EXPECT_EQ(value * Vec2(2.0F, 0.5F), Vec2(6.0F, 2.0F));
    EXPECT_EQ(value / Vec2(3.0F, 2.0F), Vec2(1.0F, 2.0F));
    EXPECT_EQ(value * 2.0F, Vec2(6.0F, 8.0F));
    EXPECT_EQ(2.0F * value, Vec2(6.0F, 8.0F));
    EXPECT_EQ(value / 2.0F, Vec2(1.5F, 2.0F));
    EXPECT_EQ(-value, Vec2(-3.0F, -4.0F));

    value += Vec2(1.0F, 1.0F);
    value -= Vec2(2.0F, 2.0F);
    value *= Vec2(2.0F, 1.0F);
    value *= 2.0F;
    value /= 4.0F;
    EXPECT_EQ(value, Vec2(2.0F, 1.5F));
}

TEST(Vec2Test, LengthDirectionAndRotation) {
    const Vec2 value{3.0F, 4.0F};
    EXPECT_FLOAT_EQ(value.getLength(), 5.0F);
    EXPECT_FLOAT_EQ(value.getLengthSquared(), 25.0F);
    EXPECT_FLOAT_EQ(value.getNormalized().getLength(), 1.0F);
    EXPECT_EQ(Vec2{}.getNormalized(), Vec2{});
    EXPECT_TRUE(Vec2{}.isZero());
    EXPECT_FALSE(value.isZero());
    EXPECT_EQ(value.getPerpendicular(), Vec2(-4.0F, 3.0F));
    EXPECT_EQ(value.clampedLength(10.0F), value);
    EXPECT_NEAR(value.clampedLength(1.0F).getLength(), 1.0F, 1e-5F);

    const Vec2 rotated = Vec2{1.0F, 0.0F}.rotated(Math::kHalfPi);
    EXPECT_NEAR(rotated.x, 0.0F, 1e-6F);
    EXPECT_NEAR(rotated.y, 1.0F, 1e-6F);
    EXPECT_NEAR(Vec2::fromAngle(Math::kPi, 2.0F).x, -2.0F, 1e-6F);
    EXPECT_NEAR(Vec2(0.0F, 1.0F).getAngle(), Math::kHalfPi, 1e-6F);
}

TEST(Vec2Test, StaticHelpers) {
    EXPECT_FLOAT_EQ(Vec2::dot(Vec2(1.0F, 2.0F), Vec2(3.0F, 4.0F)), 11.0F);
    EXPECT_FLOAT_EQ(Vec2::cross(Vec2(1.0F, 0.0F), Vec2(0.0F, 1.0F)), 1.0F);
    EXPECT_FLOAT_EQ(Vec2::distance(Vec2{}, Vec2(3.0F, 4.0F)), 5.0F);
    EXPECT_FLOAT_EQ(Vec2::distanceSquared(Vec2{}, Vec2(3.0F, 4.0F)), 25.0F);
    EXPECT_EQ(Vec2::lerp(Vec2{}, Vec2(10.0F, 20.0F), 0.5F), Vec2(5.0F, 10.0F));
    EXPECT_EQ(Vec2::min(Vec2(1.0F, 5.0F), Vec2(2.0F, 3.0F)), Vec2(1.0F, 3.0F));
    EXPECT_EQ(Vec2::max(Vec2(1.0F, 5.0F), Vec2(2.0F, 3.0F)), Vec2(2.0F, 5.0F));
    EXPECT_EQ(Vec2::floor(Vec2(1.7F, -1.2F)), Vec2(1.0F, -2.0F));
    EXPECT_EQ(Vec2::round(Vec2(1.5F, -1.4F)), Vec2(2.0F, -1.0F));
}

TEST(MathTest, ScalarHelpers) {
    EXPECT_FLOAT_EQ(Math::lerp(0.0F, 10.0F, 0.25F), 2.5F);
    EXPECT_FLOAT_EQ(Math::inverseLerp(0.0F, 10.0F, 2.5F), 0.25F);
    EXPECT_FLOAT_EQ(Math::inverseLerp(5.0F, 5.0F, 7.0F), 0.0F);
    EXPECT_FLOAT_EQ(Math::remap(5.0F, 0.0F, 10.0F, 100.0F, 200.0F), 150.0F);
    EXPECT_FLOAT_EQ(Math::saturate(2.0F), 1.0F);
    EXPECT_FLOAT_EQ(Math::smoothstep(0.0F, 1.0F, 0.5F), 0.5F);
    EXPECT_FLOAT_EQ(Math::moveToward(0.0F, 10.0F, 3.0F), 3.0F);
    EXPECT_FLOAT_EQ(Math::moveToward(10.0F, 0.0F, 3.0F), 7.0F);
    EXPECT_FLOAT_EQ(Math::moveToward(1.0F, 2.0F, 5.0F), 2.0F);
    EXPECT_FLOAT_EQ(Math::sign(-3.0F), -1.0F);
    EXPECT_FLOAT_EQ(Math::sign(0.0F), 0.0F);
    EXPECT_FLOAT_EQ(Math::sign(2.0F), 1.0F);
    EXPECT_FLOAT_EQ(Math::degrees(Math::radians(90.0F)), 90.0F);
    EXPECT_TRUE(Math::approximately(1.0F, 1.0F + 1e-7F));
    EXPECT_FALSE(Math::approximately(1.0F, 1.1F));
    EXPECT_NEAR(Math::wrapAngle(Math::kPi * 3.0F), -Math::kPi, 1e-5F);
    EXPECT_NEAR(Math::wrapAngle(-Math::kPi * 0.5F), -Math::kPi * 0.5F, 1e-5F);
    EXPECT_NEAR(Math::dampFactor(10.0F, 0.1F), 1.0F - std::exp(-1.0F), 1e-6F);
}

TEST(RectTest, EdgesAndContainment) {
    const Rect rect{10.0F, 20.0F, 100.0F, 50.0F};
    EXPECT_EQ(rect.getLeft(), 10.0F);
    EXPECT_EQ(rect.getRight(), 110.0F);
    EXPECT_EQ(rect.getTop(), 20.0F);
    EXPECT_EQ(rect.getBottom(), 70.0F);
    EXPECT_EQ(rect.getCenter(), Vec2(60.0F, 45.0F));
    EXPECT_EQ(rect.getSize(), Vec2(100.0F, 50.0F));
    EXPECT_EQ(rect.getPosition(), Vec2(10.0F, 20.0F));
    EXPECT_FLOAT_EQ(rect.getArea(), 5000.0F);
    EXPECT_FALSE(rect.isEmpty());
    EXPECT_TRUE(Rect{}.isEmpty());

    EXPECT_TRUE(rect.contains(Vec2(10.0F, 20.0F)));
    EXPECT_FALSE(rect.contains(Vec2(110.0F, 20.0F)));
    EXPECT_TRUE(rect.contains(Rect{20.0F, 30.0F, 10.0F, 10.0F}));
    EXPECT_FALSE(rect.contains(Rect{0.0F, 30.0F, 20.0F, 10.0F}));
    EXPECT_EQ(rect.clamp(Vec2(0.0F, 100.0F)), Vec2(10.0F, 70.0F));
}

TEST(RectTest, SetOperations) {
    const Rect a{0.0F, 0.0F, 10.0F, 10.0F};
    const Rect b{5.0F, 5.0F, 10.0F, 10.0F};
    EXPECT_TRUE(a.intersects(b));
    EXPECT_FALSE(a.intersects(Rect{20.0F, 20.0F, 1.0F, 1.0F}));
    EXPECT_EQ(a.intersection(b), (Rect{5.0F, 5.0F, 5.0F, 5.0F}));
    EXPECT_TRUE(a.intersection(Rect{20.0F, 20.0F, 1.0F, 1.0F}).isEmpty());
    EXPECT_EQ(a.merged(b), (Rect{0.0F, 0.0F, 15.0F, 15.0F}));
    EXPECT_EQ(a.expanded(1.0F), (Rect{-1.0F, -1.0F, 12.0F, 12.0F}));
    EXPECT_EQ(a.translated(Vec2(2.0F, 3.0F)), (Rect{2.0F, 3.0F, 10.0F, 10.0F}));
    EXPECT_EQ(a.inset(Insets{1.0F, 2.0F, 3.0F, 4.0F}), (Rect{1.0F, 2.0F, 6.0F, 4.0F}));
    EXPECT_EQ(a.inset(Insets::uniform(20.0F)).getSize(), Vec2{});
    EXPECT_EQ(Rect::fromCenter(Vec2(5.0F, 5.0F), Vec2(10.0F, 10.0F)), a);
    EXPECT_EQ(Rect::fromMinMax(Vec2{}, Vec2(10.0F, 10.0F)), a);
    EXPECT_FLOAT_EQ((Insets{1.0F, 2.0F, 3.0F, 4.0F}.getHorizontal()), 4.0F);
    EXPECT_FLOAT_EQ((Insets{1.0F, 2.0F, 3.0F, 4.0F}.getVertical()), 6.0F);
}

} // namespace haylen::math
