#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/math/Box3.hpp"
#include "haylen/math/Capsule3.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Geometry3.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Obb.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Transform3D.hpp"
#include "haylen/math/Triangle3.hpp"

namespace haylen::math {

class Geometry3Test : public ::testing::Test {
  protected:
    static Ray3 down(float x, float z) {
        return Ray3::between({x, 10.0F, z}, {x, -10.0F, z});
    }
};

TEST_F(Geometry3Test, HitsTrianglesFromBothSidesWithTheirNormal) {
    const Triangle3 floor{{-1.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -1.0F}};
    const std::optional<RayHit3> hit = Geometry3::raycast(down(0.0F, 0.0F), floor);
    ASSERT_TRUE(hit.has_value());
    EXPECT_FLOAT_EQ(hit->distance, 10.0F);
    EXPECT_EQ(hit->point, Vec3(0.0F, 0.0F, 0.0F));
    EXPECT_EQ(hit->normal, Vec3::up());

    const std::optional<RayHit3> below = Geometry3::raycast(Ray3::between({0.0F, -5.0F, 0.0F}, {0.0F, 5.0F, 0.0F}), floor);
    ASSERT_TRUE(below.has_value());
    EXPECT_EQ(below->normal, -Vec3::up());
    EXPECT_FALSE(Geometry3::raycast(down(3.0F, 0.0F), floor).has_value());
    EXPECT_FALSE(Geometry3::raycast(Ray3::between({0.0F, 10.0F, 0.0F}, {0.0F, 5.0F, 0.0F}), floor).has_value());
    EXPECT_FALSE(Geometry3::raycast(Ray3{{0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, floor).has_value());
    EXPECT_EQ(floor.getNormal(), Vec3::up());
    EXPECT_THROW((void)Triangle3{}.getNormal(), std::invalid_argument);
}

TEST_F(Geometry3Test, LetsNoRaySlipBetweenTrianglesThatShareAnEdge) {
    const Vec3 a{-1.0F, 0.0F, -1.0F};
    const Vec3 b{1.0F, 0.0F, -1.0F};
    const Vec3 c{1.0F, 0.0F, 1.0F};
    const Vec3 d{-1.0F, 0.0F, 1.0F};
    const std::array<Triangle3, 2> quad{{{a, c, b}, {a, d, c}}};

    Random random(3);
    for (int index = 0; index < 2000; ++index) {
        const float along = random.range(-0.999F, 0.999F);
        const Ray3 ray = Ray3::between({along + random.range(-1e-3F, 1e-3F), 5.0F, along}, {along, -5.0F, along});
        const bool hitsOne = Geometry3::raycast(ray, quad[0]).has_value() || Geometry3::raycast(ray, quad[1]).has_value();
        EXPECT_TRUE(hitsOne) << index;
    }
}

TEST_F(Geometry3Test, HitsBoxesAtTheFaceTheyEnter) {
    const Box3 box{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const std::optional<RayHit3> side = Geometry3::raycast(Ray3::between({-5.0F, 0.0F, 0.0F}, {5.0F, 0.0F, 0.0F}), box);
    ASSERT_TRUE(side.has_value());
    EXPECT_FLOAT_EQ(side->distance, 4.0F);
    EXPECT_EQ(side->normal, Vec3(-1.0F, 0.0F, 0.0F));

    const std::optional<RayHit3> grazing = Geometry3::raycast(Ray3::between({-5.0F, 1.0F, 0.0F}, {5.0F, 1.0F, 0.0F}), box);
    ASSERT_TRUE(grazing.has_value());
    EXPECT_FLOAT_EQ(grazing->distance, 4.0F);
    EXPECT_FALSE(Geometry3::raycast(Ray3::between({-5.0F, 1.001F, 0.0F}, {5.0F, 1.001F, 0.0F}), box).has_value());

    const std::optional<RayHit3> corner = Geometry3::raycast(Ray3::between({2.0F, 2.0F, 2.0F}, {0.0F, 0.0F, 0.0F}), box);
    ASSERT_TRUE(corner.has_value());
    EXPECT_NEAR(corner->distance, std::sqrt(3.0F), 1e-5F);

    const std::optional<RayHit3> inside = Geometry3::raycast(Ray3::between({0.0F, 0.0F, 0.0F}, {5.0F, 0.0F, 0.0F}), box);
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(inside->distance, 0.0F);
    EXPECT_TRUE(inside->normal.isZero());
    EXPECT_FALSE(Geometry3::raycast(Ray3::between({-5.0F, 0.0F, 0.0F}, {-3.0F, 0.0F, 0.0F}), box).has_value());

    const Obb turned{{10.0F, 0.0F, 0.0F}, Quat::fromAxisAngle(Vec3::up(), Math::kPi / 4.0F), {1.0F, 1.0F, 1.0F}};
    const std::optional<RayHit3> diamond = Geometry3::raycast(Ray3::between({0.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}), turned);
    ASSERT_TRUE(diamond.has_value());
    EXPECT_NEAR(diamond->distance, 10.0F - std::sqrt(2.0F), 1e-4F);
    EXPECT_TRUE(Vec3::approximately(diamond->normal, Vec3(-1.0F, 0.0F, 1.0F).getNormalized(), 1e-4F) || Vec3::approximately(diamond->normal, Vec3(-1.0F, 0.0F, -1.0F).getNormalized(), 1e-4F));
}

TEST_F(Geometry3Test, HitsSpheresPlanesAndCapsules) {
    const std::optional<RayHit3> ball = Geometry3::raycast(Ray3::between({0.0F, 0.0F, 10.0F}, {0.0F, 0.0F, -10.0F}), Sphere{{}, 2.0F});
    ASSERT_TRUE(ball.has_value());
    EXPECT_FLOAT_EQ(ball->distance, 8.0F);
    EXPECT_EQ(ball->normal, Vec3(0.0F, 0.0F, 1.0F));
    const std::optional<RayHit3> grazing = Geometry3::raycast(Ray3::between({2.0F, 0.0F, 10.0F}, {2.0F, 0.0F, -10.0F}), Sphere{{}, 2.0F});
    ASSERT_TRUE(grazing.has_value());
    EXPECT_NEAR(grazing->distance, 10.0F, 1e-4F);
    EXPECT_FALSE(Geometry3::raycast(Ray3::between({2.1F, 0.0F, 10.0F}, {2.1F, 0.0F, -10.0F}), Sphere{{}, 2.0F}).has_value());

    const Plane ground = Plane::fromPoint(Vec3::up(), {0.0F, 1.0F, 0.0F});
    const std::optional<RayHit3> land = Geometry3::raycast(down(3.0F, 4.0F), ground);
    ASSERT_TRUE(land.has_value());
    EXPECT_FLOAT_EQ(land->distance, 9.0F);
    EXPECT_EQ(land->normal, Vec3::up());
    EXPECT_FALSE(Geometry3::raycast(Ray3{{0.0F, 5.0F, 0.0F}, Vec3::right()}, ground).has_value());

    const Capsule3 body{{0.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}, 0.5F};
    const std::optional<RayHit3> side = Geometry3::raycast(Ray3::between({5.0F, 1.0F, 0.0F}, {-5.0F, 1.0F, 0.0F}), body);
    ASSERT_TRUE(side.has_value());
    EXPECT_NEAR(side->distance, 4.5F, 1e-5F);
    EXPECT_TRUE(Vec3::approximately(side->normal, Vec3::right()));
    const std::optional<RayHit3> top = Geometry3::raycast(down(0.0F, 0.0F), body);
    ASSERT_TRUE(top.has_value());
    EXPECT_NEAR(top->distance, 7.5F, 1e-5F);
    EXPECT_TRUE(Vec3::approximately(top->normal, Vec3::up()));
    EXPECT_FALSE(Geometry3::raycast(down(0.6F, 0.0F), body).has_value());
    EXPECT_EQ(Geometry3::raycast(Ray3::between({0.0F, 1.0F, 0.0F}, {5.0F, 1.0F, 0.0F}), body)->distance, 0.0F);
}

TEST_F(Geometry3Test, SeparatesOrientedBoxesOnTheirAxes) {
    const Obb first{{}, Quat::identity(), {1.0F, 1.0F, 1.0F}};
    EXPECT_TRUE(Geometry3::intersects(first, Obb{{1.9F, 0.0F, 0.0F}, Quat::identity(), {1.0F, 1.0F, 1.0F}}));
    EXPECT_FALSE(Geometry3::intersects(first, Obb{{2.1F, 0.0F, 0.0F}, Quat::identity(), {1.0F, 1.0F, 1.0F}}));

    const Obb diamond{{2.3F, 0.0F, 0.0F}, Quat::fromAxisAngle(Vec3::up(), Math::kPi / 4.0F), {1.0F, 1.0F, 1.0F}};
    EXPECT_TRUE(Geometry3::intersects(first, diamond));
    const Obb farDiamond{{2.5F, 0.0F, 0.0F}, Quat::fromAxisAngle(Vec3::up(), Math::kPi / 4.0F), {1.0F, 1.0F, 1.0F}};
    EXPECT_FALSE(Geometry3::intersects(first, farDiamond));

    EXPECT_TRUE(diamond.contains({2.3F + 1.3F, 0.0F, 0.0F}));
    EXPECT_FALSE(diamond.contains({2.3F + 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(Vec3::approximately(first.getClosestPoint({5.0F, 0.5F, -7.0F}), {1.0F, 0.5F, -1.0F}));
    EXPECT_TRUE(Vec3::approximately(diamond.getBounds().max, {2.3F + std::sqrt(2.0F), 1.0F, std::sqrt(2.0F)}, 1e-5F));
}

TEST_F(Geometry3Test, BoxesMergeTransformAndStayStableAtAnyScale) {
    const Box3 box = Box3::fromCenter({1.0F, 2.0F, 3.0F}, {2.0F, 4.0F, 6.0F});
    EXPECT_EQ(box.min, Vec3(0.0F, 0.0F, 0.0F));
    EXPECT_EQ(box.max, Vec3(2.0F, 4.0F, 6.0F));
    EXPECT_TRUE(box.contains(Vec3{2.0F, 4.0F, 6.0F}));
    EXPECT_TRUE(box.intersects(Box3{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}}));
    EXPECT_FALSE(box.intersects(Box3{{2.1F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}}));
    EXPECT_EQ(box.merged(Vec3{-1.0F, 5.0F, 0.0F}), Box3({-1.0F, 0.0F, 0.0F}, {2.0F, 5.0F, 6.0F}));
    EXPECT_EQ(box.expanded(1.0F).getSize(), Vec3(4.0F, 6.0F, 8.0F));

    const Mat4 turn = Mat4::compose({10.0F, 0.0F, 0.0F}, Quat::fromAxisAngle(Vec3::up(), Math::kHalfPi), {1.0F, 1.0F, 1.0F});
    const Box3 turned = Box3{{0.0F, 0.0F, 0.0F}, {2.0F, 1.0F, 1.0F}}.transformed(turn);
    EXPECT_TRUE(Vec3::approximately(turned.min, {10.0F, 0.0F, -2.0F}, 1e-5F));
    EXPECT_TRUE(Vec3::approximately(turned.max, {11.0F, 1.0F, 0.0F}, 1e-5F));
    EXPECT_THROW((void)box.transformed(Mat4::perspective({})), std::invalid_argument);

    const std::vector<Vec3> huge{{1e30F, -1e30F, 3e29F}, {-2e30F, 5e29F, -1e30F}};
    const Box3 hugeBounds = Box3::bounds(huge);
    EXPECT_EQ(hugeBounds.min, Vec3(-2e30F, -1e30F, -1e30F));
    EXPECT_EQ(hugeBounds.max, Vec3(1e30F, 5e29F, 3e29F));
    EXPECT_TRUE(hugeBounds.getCenter().isFinite());
    const std::vector<Vec3> tiny{{1e-30F, 0.0F, -1e-30F}, {-1e-30F, 2e-30F, 1e-30F}};
    EXPECT_TRUE(Box3::bounds(tiny).contains(Vec3{0.0F, 1e-30F, 0.0F}));
    EXPECT_THROW((void)Box3::bounds({}), std::invalid_argument);
}

TEST_F(Geometry3Test, CullsNothingThatTheViewSees) {
    const Mat4 projection = Mat4::perspective({.fieldOfView = Math::radians(60.0F), .aspect = 1.5F, .nearPlane = 0.5F, .farPlane = 100.0F});
    const Mat4 view = Mat4::lookAt({0.0F, 2.0F, 10.0F}, {0.0F, 2.0F, 0.0F});
    const Mat4 viewProjection = projection * view;
    const Mat4 back = viewProjection.getInverse();
    const Frustum frustum = Frustum::fromMatrix(viewProjection, Mat4::DepthRange::ZeroToOne);
    ASSERT_EQ(frustum.getPlanes().size(), 6U);

    Random random(11);
    for (int index = 0; index < 1000; ++index) {
        const Vec3 ndc{random.range(-1.0F, 1.0F), random.range(-1.0F, 1.0F), random.range(0.0F, 1.0F)};
        const Vec3 inside = back.apply(ndc);
        EXPECT_TRUE(frustum.contains(inside)) << index;
        EXPECT_NE(frustum.classify(Sphere{inside, 0.01F}), Frustum::Containment::Outside) << index;
        EXPECT_NE(frustum.classify(Box3::fromCenter(inside, {0.02F, 0.02F, 0.02F})), Frustum::Containment::Outside) << index;

        // A point on a face of the frustum, which rounding may put a hair outside, still counts as inside.
        Vec3 onFace = ndc;
        const int face = index % 6;
        (face < 2 ? onFace.x : (face < 4 ? onFace.y : onFace.z)) = face % 2 == 0 ? (face < 4 ? -1.0F : 0.0F) : 1.0F;
        EXPECT_TRUE(frustum.contains(back.apply(onFace))) << index;
    }

    EXPECT_EQ(frustum.classify(Sphere{{0.0F, 2.0F, 0.0F}, 1.0F}), Frustum::Containment::Inside);
    EXPECT_EQ(frustum.classify(Sphere{{0.0F, 2.0F, 20.0F}, 1.0F}), Frustum::Containment::Outside);
    EXPECT_EQ(frustum.classify(Sphere{{0.0F, 2.0F, 9.5F}, 1.0F}), Frustum::Containment::Intersecting);
    EXPECT_EQ(frustum.classify(Box3::fromCenter({0.0F, 2.0F, -200.0F}, {1.0F, 1.0F, 1.0F})), Frustum::Containment::Outside);
    EXPECT_FALSE(frustum.contains({50.0F, 2.0F, 0.0F}));

    const Frustum endless = Frustum::fromMatrix(Mat4::perspective({.nearPlane = 0.5F}) * view, Mat4::DepthRange::ZeroToOne);
    EXPECT_EQ(endless.getPlanes().size(), 5U);
    EXPECT_TRUE(endless.contains({0.0F, 2.0F, -1e6F}));

    const Mat4 openGl = Mat4::perspective({.nearPlane = 0.5F, .farPlane = 100.0F, .depthRange = Mat4::DepthRange::NegativeOneToOne, .reversedDepth = false}) * view;
    const Frustum openGlFrustum = Frustum::fromMatrix(openGl, Mat4::DepthRange::NegativeOneToOne);
    EXPECT_TRUE(openGlFrustum.contains({0.0F, 2.0F, 9.0F}));
    EXPECT_FALSE(openGlFrustum.contains({0.0F, 2.0F, 9.9F}));
    EXPECT_EQ(Frustum::containmentFromName("intersecting"), Frustum::Containment::Intersecting);
}

TEST_F(Geometry3Test, FindsTheClosestPoints) {
    const Triangle3 triangle{{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -4.0F}};
    EXPECT_EQ(triangle.getClosestPoint({1.0F, 5.0F, -1.0F}), Vec3(1.0F, 0.0F, -1.0F));
    EXPECT_EQ(triangle.getClosestPoint({-3.0F, 1.0F, 3.0F}), Vec3(0.0F, 0.0F, 0.0F));
    EXPECT_EQ(triangle.getClosestPoint({2.0F, 0.0F, 3.0F}), Vec3(2.0F, 0.0F, 0.0F));
    EXPECT_TRUE(Vec3::approximately(triangle.getClosestPoint({4.0F, 0.0F, -4.0F}), {2.0F, 0.0F, -2.0F}));
    EXPECT_FLOAT_EQ(triangle.getArea(), 8.0F);

    const Segment3 first{{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}};
    const Segment3 second{{5.0F, 3.0F, -1.0F}, {5.0F, 3.0F, 1.0F}};
    const auto [onFirst, onSecond] = Geometry3::closestPoints(first, second);
    EXPECT_TRUE(Vec3::approximately(onFirst, {5.0F, 0.0F, 0.0F}));
    EXPECT_TRUE(Vec3::approximately(onSecond, {5.0F, 3.0F, 0.0F}));
    EXPECT_EQ(first.getClosestPoint({-4.0F, 1.0F, 0.0F}), Vec3(0.0F, 0.0F, 0.0F));
    const auto [parallelFirst, parallelSecond] = Geometry3::closestPoints(first, Segment3{{12.0F, 1.0F, 0.0F}, {20.0F, 1.0F, 0.0F}});
    EXPECT_TRUE(Vec3::approximately(parallelFirst, {10.0F, 0.0F, 0.0F}));
    EXPECT_TRUE(Vec3::approximately(parallelSecond, {12.0F, 1.0F, 0.0F}));

    EXPECT_TRUE(Sphere({}, 1.0F).intersects(Box3{{0.5F, 0.5F, 0.5F}, {2.0F, 2.0F, 2.0F}}));
    EXPECT_FALSE(Sphere({}, 1.0F).intersects(Box3{{0.7F, 0.7F, 0.7F}, {2.0F, 2.0F, 2.0F}}));
    EXPECT_TRUE(Capsule3({}, {0.0F, 2.0F, 0.0F}, 0.5F).contains({0.4F, 1.0F, 0.0F}));
    const Obb placed = Obb::fromTransform(Box3{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}}, Transform3D{{5.0F, 0.0F, 0.0F}, Quat::identity(), {-2.0F, 1.0F, 1.0F}});
    EXPECT_EQ(placed.halfExtents, Vec3(2.0F, 1.0F, 1.0F));
    EXPECT_EQ(placed.center, Vec3(5.0F, 0.0F, 0.0F));
}

} // namespace haylen::math
