#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/math/Math.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::math {

class QuatTest : public ::testing::Test {
  protected:
    static constexpr std::array<Quat::EulerOrder, 6> kOrders{Quat::EulerOrder::Xyz, Quat::EulerOrder::Xzy, Quat::EulerOrder::Yxz, Quat::EulerOrder::Yzx, Quat::EulerOrder::Zxy, Quat::EulerOrder::Zyx};

    static std::string errorOf(const auto& call) {
        try {
            call();
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }

    // The angle that the order turns in the middle, which reaches its poles at a quarter turn.
    static float& middleAngle(Vec3& angles, Quat::EulerOrder order) {
        const char axis = Quat::eulerOrderName(order)[1];
        return axis == 'x' ? angles.x : (axis == 'y' ? angles.y : angles.z);
    }
};

TEST_F(QuatTest, TurnsByTheRightHandAroundItsAxis) {
    const Quat quarter = Quat::fromAxisAngle(Vec3::up(), Math::kHalfPi);
    EXPECT_TRUE(Vec3::approximately(quarter.rotate(Vec3::right()), Vec3::forward()));
    EXPECT_TRUE(Vec3::approximately(quarter.rotate(Vec3::up()), Vec3::up()));
    EXPECT_NEAR(quarter.getLength(), 1.0F, 1e-6F);
    EXPECT_NEAR(quarter.getAngle(), Math::kHalfPi, 1e-6F);
    EXPECT_TRUE(Vec3::approximately(quarter.getAxis(), Vec3::up()));
    EXPECT_TRUE(Quat::approximately(quarter * quarter.getInverse(), Quat::identity()));
    EXPECT_TRUE(Vec3::approximately((quarter * quarter).rotate(Vec3::right()), -Vec3::right()));

    const Quat tilt = Quat::fromAxisAngle(Vec3::right(), 0.4F);
    const Vec3 point{1.0F, 2.0F, 3.0F};
    EXPECT_TRUE(Vec3::approximately((quarter * tilt).rotate(point), quarter.rotate(tilt.rotate(point))));
    EXPECT_TRUE(Vec3::approximately(quarter.toMatrix() * point, quarter.rotate(point)));
    EXPECT_TRUE(Quat::approximately(Quat::fromMatrix((quarter * tilt).toMatrix()), quarter * tilt));
}

TEST_F(QuatTest, RefusesRotationsWithoutDirection) {
    EXPECT_EQ(errorOf([] { (void)Quat::fromAxisAngle({}, 1.0F); }), "The vector has no direction, because its length is zero.");
    EXPECT_EQ(errorOf([] { (void)Quat::identity().getAxis(); }), "The rotation does not turn, so it has no axis.");
    EXPECT_EQ(errorOf([] { (void)Quat{0.0F, 0.0F, 0.0F, 0.0F}.getNormalized(); }), "The quaternion holds no rotation, because its length is zero or not a finite number.");
    EXPECT_EQ(errorOf([] { (void)Quat{0.0F, 0.0F, 0.0F, 0.0F}.getInverse(); }), "The quaternion has no inverse, because its length is zero or not a finite number.");
    EXPECT_THROW((void)Quat::between({}, Vec3::up()), std::invalid_argument);
    EXPECT_EQ(errorOf([] { (void)Quat::lookRotation({}); }), "A rotation that looks along a direction needs a direction with a finite length above zero.");
    EXPECT_EQ(errorOf([] { (void)Quat::lookRotation({0.0F, 3.0F, 0.0F}); }), "The up direction needs a finite length above zero and must not run along the direction to look along, or it cannot choose how the view turns around that direction.");
}

TEST_F(QuatTest, SlerpTakesTheShortArcAndKeepsItsEnds) {
    const Quat from = Quat::identity();
    const Quat quarter = Quat::fromAxisAngle(Vec3::up(), Math::kHalfPi);
    const Quat opposite{-quarter.x, -quarter.y, -quarter.z, -quarter.w};

    EXPECT_EQ(Quat::slerp(from, opposite, 0.0F), from);
    EXPECT_EQ(Quat::slerp(from, opposite, 1.0F), quarter);
    EXPECT_TRUE(Quat::approximately(Quat::slerp(from, opposite, 0.5F), Quat::fromAxisAngle(Vec3::up(), Math::kPi / 4.0F)));
    EXPECT_NEAR(Quat::slerp(from, opposite, 0.5F).w, std::cos(Math::kPi / 8.0F), 1e-6F);
    EXPECT_NEAR(Quat::angleBetween(from, Quat::slerp(from, quarter, 0.25F)), Math::kPi / 8.0F, 1e-5F);
    EXPECT_NEAR(Quat::slerp(from, quarter, 0.3F).getLength(), 1.0F, 1e-6F);

    const Quat close = Quat::fromAxisAngle(Vec3::right(), 1e-4F);
    EXPECT_TRUE(Quat::approximately(Quat::slerp(from, close, 0.5F), Quat::fromAxisAngle(Vec3::right(), 5e-5F)));
    EXPECT_TRUE(Quat::approximately(Quat::nlerp(from, opposite, 0.5F), Quat::fromAxisAngle(Vec3::up(), Math::kPi / 4.0F)));
    EXPECT_NEAR(Quat::angleBetween(quarter, opposite), 0.0F, 1e-3F);
}

TEST_F(QuatTest, EulerAnglesRoundTripInEveryOrder) {
    Random random(42);
    for (const Quat::EulerOrder order : kOrders) {
        for (int index = 0; index < 200; ++index) {
            Vec3 angles{random.range(-Math::kPi, Math::kPi), random.range(-Math::kPi, Math::kPi), random.range(-Math::kPi, Math::kPi)};
            middleAngle(angles, order) = random.range(-1.5F, 1.5F);
            const Quat rotation = Quat::fromEuler(angles, order);
            const Vec3 back = rotation.toEuler(order);
            EXPECT_TRUE(Vec3::approximately(back, angles, 1e-3F)) << Quat::eulerOrderName(order) << " " << index;
            EXPECT_TRUE(Quat::approximately(Quat::fromEuler(back, order), rotation, 1e-4F)) << Quat::eulerOrderName(order) << " " << index;
        }
    }
}

TEST_F(QuatTest, EulerAnglesKeepTheRotationAtThePoles) {
    for (const Quat::EulerOrder order : kOrders) {
        for (const float pole : {Math::kHalfPi, -Math::kHalfPi, Math::kHalfPi - 1e-4F}) {
            Vec3 angles{0.7F, -0.3F, 1.2F};
            middleAngle(angles, order) = pole;
            const Quat rotation = Quat::fromEuler(angles, order);
            EXPECT_TRUE(Quat::approximately(Quat::fromEuler(rotation.toEuler(order), order), rotation, 1e-3F)) << Quat::eulerOrderName(order) << " " << pole;
        }
    }
}

TEST_F(QuatTest, NamesTheOrderOfItsTurns) {
    const Vec3 angles{0.3F, 0.5F, 0.7F};
    const Quat x = Quat::fromAxisAngle(Vec3::right(), angles.x);
    const Quat y = Quat::fromAxisAngle(Vec3::up(), angles.y);
    const Quat z = Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, angles.z);
    EXPECT_TRUE(Quat::approximately(Quat::fromEuler(angles, Quat::EulerOrder::Yxz), y * x * z));
    EXPECT_TRUE(Quat::approximately(Quat::fromEuler(angles, Quat::EulerOrder::Zyx), z * y * x));
    EXPECT_EQ(Quat::eulerOrderFromName("xzy"), Quat::EulerOrder::Xzy);
    EXPECT_FALSE(Quat::eulerOrderFromName("XYZ").has_value());
}

TEST_F(QuatTest, TurnsOneDirectionOntoAnother) {
    const Quat turn = Quat::between({1.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 2.0F});
    EXPECT_TRUE(Vec3::approximately(turn.rotate(Vec3::right()), Vec3(0.0F, 1.0F, 1.0F).getNormalized()));
    EXPECT_TRUE(Quat::approximately(Quat::between(Vec3::up(), Vec3::up()), Quat::identity()));

    const Quat back = Quat::between(Vec3::right(), -Vec3::right());
    EXPECT_TRUE(Vec3::approximately(back.rotate(Vec3::right()), -Vec3::right()));
    EXPECT_NEAR(back.getAngle(), Math::kPi, 1e-5F);

    const Quat look = Quat::lookRotation({1.0F, 0.0F, -1.0F});
    EXPECT_TRUE(Vec3::approximately(look.rotate(Vec3::forward()), Vec3(1.0F, 0.0F, -1.0F).getNormalized()));
    EXPECT_TRUE(Vec3::approximately(look.rotate(Vec3::up()), Vec3::up()));
}

TEST_F(QuatTest, SplitsIntoSwingAndTwist) {
    const Quat twist = Quat::fromAxisAngle(Vec3::up(), 0.8F);
    const Quat swing = Quat::fromAxisAngle(Vec3::right(), 0.5F);
    const Quat rotation = swing * twist;
    const Quat::SwingTwist parts = rotation.getSwingTwist({0.0F, 3.0F, 0.0F});
    EXPECT_TRUE(Quat::approximately(parts.twist, twist));
    EXPECT_TRUE(Quat::approximately(parts.swing, swing));
    EXPECT_TRUE(Quat::approximately(parts.swing * parts.twist, rotation));

    const Quat flip = Quat::fromAxisAngle(Vec3::right(), Math::kPi);
    EXPECT_TRUE(Quat::approximately(flip.getSwingTwist(Vec3::up()).twist, Quat::identity()));
}

} // namespace haylen::math
