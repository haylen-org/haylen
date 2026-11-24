#include <gtest/gtest.h>

#include "haylen/math/Cylindrical.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Spherical.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

class SphericalCoordinatesTest : public ::testing::Test {
  protected:
    static void expectNear(Vec3 actual, Vec3 expected, float tolerance) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
};

// The polar angle runs down from the up axis and the azimuth turns by the right hand from positive z, so a quarter turn reaches positive x.
TEST_F(SphericalCoordinatesTest, ConvertsSphericalCoordinatesBothWays) {
    expectNear(Spherical{.radius = 2.0F, .polar = Math::kHalfPi, .azimuth = 0.0F}.toVector(), {0.0F, 0.0F, 2.0F}, 1e-6F);
    expectNear(Spherical{.radius = 2.0F, .polar = Math::kHalfPi, .azimuth = Math::kHalfPi}.toVector(), {2.0F, 0.0F, 0.0F}, 1e-6F);
    expectNear(Spherical{.radius = 2.0F, .polar = Math::kHalfPi, .azimuth = Math::kPi}.toVector(), {0.0F, 0.0F, -2.0F}, 1e-6F);

    const Spherical front = Spherical::fromVector({0.0F, 0.0F, 3.0F});
    EXPECT_EQ(front.radius, 3.0F);
    EXPECT_NEAR(front.polar, Math::kHalfPi, 1e-6F);
    EXPECT_EQ(front.azimuth, 0.0F);

    // The poles have no azimuth of their own, so they come back with an azimuth of 0.
    const Spherical top = Spherical::fromVector({0.0F, 5.0F, 0.0F});
    EXPECT_EQ(top.polar, 0.0F);
    EXPECT_EQ(top.azimuth, 0.0F);
    expectNear(top.toVector(), {0.0F, 5.0F, 0.0F}, 1e-6F);
    const Spherical bottom = Spherical::fromVector({0.0F, -5.0F, 0.0F});
    EXPECT_NEAR(bottom.polar, Math::kPi, 1e-6F);
    expectNear(bottom.toVector(), {0.0F, -5.0F, 0.0F}, 1e-5F);

    const Spherical origin = Spherical::fromVector({});
    EXPECT_EQ(origin.radius, 0.0F);
    EXPECT_EQ(origin.polar, 0.0F);
    EXPECT_EQ(origin.azimuth, 0.0F);

    for (const Vec3 point : {Vec3{1.0F, 2.0F, 3.0F}, Vec3{-4.0F, -0.5F, 2.0F}, Vec3{-1.0F, 7.0F, -9.0F}, Vec3{0.0F, -2.0F, -1.0F}}) {
        expectNear(Spherical::fromVector(point).toVector(), point, 1e-5F);
    }
}

TEST_F(SphericalCoordinatesTest, ConvertsCylindricalCoordinatesBothWays) {
    expectNear(Cylindrical{.radius = 3.0F, .azimuth = Math::kHalfPi, .height = 2.0F}.toVector(), {3.0F, 2.0F, 0.0F}, 1e-6F);

    const Cylindrical onAxis = Cylindrical::fromVector({0.0F, -4.0F, 0.0F});
    EXPECT_EQ(onAxis.radius, 0.0F);
    EXPECT_EQ(onAxis.azimuth, 0.0F);
    EXPECT_EQ(onAxis.height, -4.0F);

    const Cylindrical front = Cylindrical::fromVector({0.0F, 1.0F, 2.0F});
    EXPECT_EQ(front.azimuth, 0.0F);
    EXPECT_EQ(front.radius, 2.0F);

    for (const Vec3 point : {Vec3{1.0F, 2.0F, 3.0F}, Vec3{-4.0F, -0.5F, 2.0F}, Vec3{-1.0F, 7.0F, -9.0F}}) {
        expectNear(Cylindrical::fromVector(point).toVector(), point, 1e-5F);
    }
}

} // namespace haylen::math
