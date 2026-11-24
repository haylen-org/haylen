#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "haylen/math/Mat3.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::math {

class MatrixTest : public ::testing::Test {
  protected:
    static Mat4 randomPlacement(Random& random) {
        const Vec3 axis{random.range(-1.0F, 1.0F), random.range(-1.0F, 1.0F), random.range(0.1F, 1.0F)};
        const Quat rotation = Quat::fromAxisAngle(axis, random.range(-Math::kPi, Math::kPi));
        const Vec3 scale{random.range(0.1F, 10.0F), random.range(0.1F, 10.0F), random.range(0.1F, 10.0F)};
        const Vec3 translation{random.range(-100.0F, 100.0F), random.range(-100.0F, 100.0F), random.range(-100.0F, 100.0F)};
        return Mat4::compose(translation, rotation, scale);
    }

    static std::string errorOf(const auto& call) {
        try {
            call();
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }
};

TEST_F(MatrixTest, Mat3MultipliesTransposesAndInverts) {
    const Mat3 matrix = Mat3::fromColumns({2.0F, 0.0F, 1.0F}, {0.0F, 3.0F, 0.0F}, {1.0F, 0.0F, 4.0F});
    EXPECT_EQ(matrix.get(0, 2), 1.0F);
    EXPECT_EQ(matrix.getRow(0), Vec3(2.0F, 0.0F, 1.0F));
    EXPECT_EQ(matrix * Vec3(1.0F, 1.0F, 1.0F), Vec3(3.0F, 3.0F, 5.0F));
    EXPECT_EQ(matrix.getTransposed().getColumn(0), matrix.getRow(0));
    EXPECT_FLOAT_EQ(matrix.getDeterminant(), 21.0F);
    EXPECT_TRUE(Mat3::approximately(matrix * matrix.getInverse(), Mat3::identity()));
    EXPECT_EQ(Mat3::scaling({2.0F, 3.0F, 4.0F}) * Mat3::identity(), Mat3::scaling({2.0F, 3.0F, 4.0F}));
}

TEST_F(MatrixTest, RefusesToInvertSingularMatrices) {
    const Mat3 flat = Mat3::fromColumns({1.0F, 2.0F, 3.0F}, {2.0F, 4.0F, 6.0F}, {0.0F, 0.0F, 1.0F});
    EXPECT_FALSE(flat.isInvertible());
    EXPECT_EQ(errorOf([&] { (void)flat.getInverse(); }), "The matrix has no inverse, because it squashes space flat and its determinant is zero.");

    const Mat4 squashed = Mat4::compose({1.0F, 2.0F, 3.0F}, Quat::fromAxisAngle({1.0F, 1.0F, 0.0F}, 0.7F), {1.0F, 0.0F, 2.0F});
    EXPECT_FALSE(squashed.isInvertible());
    EXPECT_THROW((void)squashed.getInverse(), std::invalid_argument);
    EXPECT_THROW((void)squashed.getAffineInverse(), std::invalid_argument);
    EXPECT_THROW((void)squashed.getNormalMatrix(), std::invalid_argument);

    Mat4 projective = Mat4::identity();
    projective.values[3] = 1.0F;
    projective.values[15] = 0.0F;
    projective.values[0] = 0.0F;
    EXPECT_FALSE(projective.isInvertible());
    EXPECT_THROW((void)projective.getInverse(), std::invalid_argument);
    EXPECT_EQ(errorOf([&] { (void)projective.getAffineInverse(); }), "The matrix is not affine, because its last row is not 0, 0, 0 and 1.");
}

TEST_F(MatrixTest, InvertsAThousandSeededPlacements) {
    Random random(20261123);
    for (int index = 0; index < 1000; ++index) {
        const Mat4 placement = randomPlacement(random);
        ASSERT_TRUE(placement.isInvertible());
        EXPECT_TRUE(Mat4::approximately(placement * placement.getInverse(), Mat4::identity(), 1e-4F)) << index;
        EXPECT_TRUE(Mat4::approximately(placement.getAffineInverse() * placement, Mat4::identity(), 1e-4F)) << index;
    }
}

TEST_F(MatrixTest, TranslatesPointsButNotDirections) {
    const Mat4 move = Mat4::compose({1.0F, 2.0F, 3.0F}, Quat::identity(), {1.0F, 1.0F, 1.0F});
    EXPECT_EQ(move.apply({4.0F, 0.0F, 0.0F}), Vec3(5.0F, 2.0F, 3.0F));
    EXPECT_EQ(move.applyVector({4.0F, 0.0F, 0.0F}), Vec3(4.0F, 0.0F, 0.0F));
    EXPECT_EQ(move.getTranslation(), Vec3(1.0F, 2.0F, 3.0F));
    EXPECT_TRUE(move.isAffine());
    EXPECT_EQ(move * Vec4(4.0F, 0.0F, 0.0F, 1.0F), Vec4(5.0F, 2.0F, 3.0F, 1.0F));
    EXPECT_EQ(move * Vec4(4.0F, 0.0F, 0.0F, 0.0F), Vec4(4.0F, 0.0F, 0.0F, 0.0F));

    const Mat4 stretch = Mat4::compose({}, Quat::identity(), {2.0F, 1.0F, 1.0F});
    const Mat3 normals = stretch.getNormalMatrix();
    EXPECT_EQ(normals * Vec3(1.0F, 0.0F, 0.0F), Vec3(0.5F, 0.0F, 0.0F));
}

TEST_F(MatrixTest, DecomposesIntoTheTransformThatComposedIt) {
    const Quat rotation = Quat::fromAxisAngle({0.3F, 1.0F, -0.2F}, 1.1F);
    const Mat4 placement = Mat4::compose({4.0F, -5.0F, 6.0F}, rotation, {2.0F, 3.0F, 0.5F});
    const Mat4::Decomposition parts = placement.decompose();
    EXPECT_TRUE(Vec3::approximately(parts.translation, {4.0F, -5.0F, 6.0F}));
    EXPECT_TRUE(Vec3::approximately(parts.scale, {2.0F, 3.0F, 0.5F}, 1e-5F));
    EXPECT_TRUE(Quat::approximately(parts.rotation, rotation, 1e-5F));

    const Mat4 mirrored = Mat4::compose({1.0F, 0.0F, 0.0F}, rotation, {-2.0F, 3.0F, 0.5F});
    const Mat4::Decomposition mirroredParts = mirrored.decompose();
    EXPECT_LT(mirroredParts.scale.x, 0.0F);
    EXPECT_TRUE(Mat4::approximately(Mat4::compose(mirroredParts.translation, mirroredParts.rotation, mirroredParts.scale), mirrored, 1e-5F));

    EXPECT_EQ(errorOf([] { (void)Mat4::compose({}, Quat::identity(), {1.0F, 0.0F, 1.0F}).decompose(); }), "The matrix scales an axis to zero or to infinity, so it holds no rotation.");
    EXPECT_THROW((void)Mat4::perspective({}).decompose(), std::invalid_argument);
}

TEST_F(MatrixTest, LooksFromTheEyeAlongNegativeZ) {
    const Mat4 view = Mat4::lookAt({0.0F, 2.0F, 10.0F}, {0.0F, 2.0F, 0.0F});
    EXPECT_TRUE(Vec3::approximately(view.apply({0.0F, 2.0F, 10.0F}), {}));
    EXPECT_TRUE(Vec3::approximately(view.apply({0.0F, 2.0F, 0.0F}), {0.0F, 0.0F, -10.0F}));
    EXPECT_TRUE(Vec3::approximately(view.apply({1.0F, 2.0F, 10.0F}), {1.0F, 0.0F, 0.0F}));

    EXPECT_EQ(errorOf([] { (void)Mat4::lookAt({1.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}); }), "The eye and the target of a view are the same point, so the view has no direction.");
    EXPECT_THROW((void)Mat4::lookAt({}, {0.0F, 5.0F, 0.0F}), std::invalid_argument);
}

TEST_F(MatrixTest, ProjectsAndUnprojectsWithReversedDepth) {
    const Mat4 projection = Mat4::perspective({.fieldOfView = Math::radians(60.0F), .aspect = 16.0F / 9.0F, .nearPlane = 0.1F, .farPlane = 100.0F});
    EXPECT_NEAR(projection.apply({0.0F, 0.0F, -0.1F}).z, 1.0F, 1e-6F);
    EXPECT_NEAR(projection.apply({0.0F, 0.0F, -100.0F}).z, 0.0F, 1e-6F);

    const Mat4 inverse = projection.getInverse();
    Random random(7);
    for (int index = 0; index < 200; ++index) {
        const Vec3 point{random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F), random.range(-99.0F, -0.2F)};
        const Vec3 back = inverse.apply(projection.apply(point));
        EXPECT_NEAR(Vec3::distance(back, point), 0.0F, 1e-3F * -point.z) << index;
    }

    const Mat4 infinite = Mat4::perspective({.nearPlane = 0.5F});
    EXPECT_NEAR(infinite.apply({0.0F, 0.0F, -0.5F}).z, 1.0F, 1e-6F);
    EXPECT_GT(infinite.apply({0.0F, 0.0F, -1e30F}).z, 0.0F);
    EXPECT_LT(infinite.apply({0.0F, 0.0F, -1e30F}).z, 1e-20F);
    EXPECT_GT(infinite.apply({0.0F, 0.0F, -10.0F}).z, infinite.apply({0.0F, 0.0F, -11.0F}).z);
}

TEST_F(MatrixTest, BuildsTheDepthOfEveryRange) {
    const Mat4::Perspective forward{.nearPlane = 1.0F, .farPlane = 10.0F, .depthRange = Mat4::DepthRange::ZeroToOne, .reversedDepth = false};
    EXPECT_NEAR(Mat4::perspective(forward).apply({0.0F, 0.0F, -1.0F}).z, 0.0F, 1e-6F);
    EXPECT_NEAR(Mat4::perspective(forward).apply({0.0F, 0.0F, -10.0F}).z, 1.0F, 1e-6F);

    const Mat4::Perspective openGl{.nearPlane = 1.0F, .farPlane = 10.0F, .depthRange = Mat4::DepthRange::NegativeOneToOne, .reversedDepth = false};
    EXPECT_NEAR(Mat4::perspective(openGl).apply({0.0F, 0.0F, -1.0F}).z, -1.0F, 1e-6F);
    EXPECT_NEAR(Mat4::perspective(openGl).apply({0.0F, 0.0F, -10.0F}).z, 1.0F, 1e-6F);
    EXPECT_TRUE(Mat4::approximately(Mat4::depthRangeConversion(Mat4::DepthRange::ZeroToOne, Mat4::DepthRange::NegativeOneToOne) * Mat4::perspective(forward), Mat4::perspective(openGl)));
    EXPECT_TRUE(Mat4::approximately(Mat4::depthRangeConversion(Mat4::DepthRange::NegativeOneToOne, Mat4::DepthRange::ZeroToOne) * Mat4::perspective(openGl), Mat4::perspective(forward)));

    const Mat4::Perspective reversedOpenGl{.nearPlane = 1.0F, .farPlane = 10.0F, .depthRange = Mat4::DepthRange::NegativeOneToOne, .reversedDepth = true};
    EXPECT_NEAR(Mat4::perspective(reversedOpenGl).apply({0.0F, 0.0F, -1.0F}).z, 1.0F, 1e-6F);
    EXPECT_NEAR(Mat4::perspective(reversedOpenGl).apply({0.0F, 0.0F, -10.0F}).z, -1.0F, 1e-6F);

    const Mat4 box = Mat4::orthographic({.left = -2.0F, .right = 2.0F, .bottom = -1.0F, .top = 1.0F, .nearPlane = 1.0F, .farPlane = 5.0F});
    EXPECT_TRUE(Vec3::approximately(box.apply({2.0F, 1.0F, -1.0F}), {1.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(Vec3::approximately(box.apply({-2.0F, -1.0F, -5.0F}), {-1.0F, -1.0F, 0.0F}));
}

TEST_F(MatrixTest, AsymmetricViewsMatchTheirAngles) {
    const float half = Math::radians(30.0F);
    const Mat4 symmetric = Mat4::asymmetricPerspective({.left = -half, .right = half, .bottom = -half, .top = half, .nearPlane = 0.1F, .farPlane = 50.0F});
    EXPECT_TRUE(Mat4::approximately(symmetric, Mat4::perspective({.fieldOfView = half * 2.0F, .aspect = 1.0F, .nearPlane = 0.1F, .farPlane = 50.0F})));

    const Mat4 eye = Mat4::asymmetricPerspective({.left = -0.9F, .right = 0.5F, .bottom = -0.7F, .top = 0.8F, .nearPlane = 0.1F});
    EXPECT_NEAR(eye.apply({std::tan(-0.9F) * 3.0F, 0.0F, -3.0F}).x, -1.0F, 1e-5F);
    EXPECT_NEAR(eye.apply({std::tan(0.5F) * 3.0F, 0.0F, -3.0F}).x, 1.0F, 1e-5F);
    EXPECT_NEAR(eye.apply({0.0F, std::tan(0.8F) * 3.0F, -3.0F}).y, 1.0F, 1e-5F);
}

TEST_F(MatrixTest, RefusesProjectionsOutOfRange) {
    EXPECT_EQ(errorOf([] { (void)Mat4::perspective({.fieldOfView = 0.0F}); }), "The field of view of a perspective must be an angle above 0 and below pi radians.");
    EXPECT_EQ(errorOf([] { (void)Mat4::perspective({.aspect = -1.0F}); }), "The aspect of a perspective must be a finite number above 0.");
    EXPECT_EQ(errorOf([] { (void)Mat4::perspective({.nearPlane = 0.0F}); }), "The near plane of a perspective must be at a finite distance above 0.");
    EXPECT_EQ(errorOf([] { (void)Mat4::perspective({.nearPlane = 2.0F, .farPlane = 1.0F}); }), "The far plane of a perspective must lie beyond its near plane.");
    EXPECT_EQ(errorOf([] { (void)Mat4::asymmetricPerspective({.left = 0.5F, .right = 0.2F}); }), "The angles of an asymmetric perspective must lie within a quarter turn of the direction it looks along, with left below right and bottom below top.");
    EXPECT_THROW((void)Mat4::asymmetricPerspective({.top = Math::kHalfPi}), std::invalid_argument);
    EXPECT_EQ(errorOf([] { (void)Mat4::orthographic({.left = 1.0F, .right = 1.0F}); }), "The bounds of an orthographic projection must be finite numbers, with left apart from right, bottom apart from top and the near plane apart from the far plane.");
    EXPECT_THROW((void)Mat4::orthographic({.farPlane = std::numeric_limits<float>::infinity()}), std::invalid_argument);
}

TEST_F(MatrixTest, ClipsTheViewAtAnObliquePlane) {
    const Mat4 projection = Mat4::perspective({.nearPlane = 0.1F, .farPlane = 100.0F});
    const Plane mirror = Plane::fromPoint({0.2F, 0.0F, -1.0F}, {0.0F, 0.0F, -5.0F});
    const Mat4 oblique = projection.getObliqueNearPlane(mirror, Mat4::DepthRange::ZeroToOne, true);

    const Vec3 onPlane = mirror.getClosestPoint({0.5F, 0.3F, -5.0F});
    EXPECT_NEAR(oblique.apply(onPlane).z, 1.0F, 1e-4F);
    EXPECT_GT(oblique.apply({0.0F, 0.0F, -3.0F}).z, 1.0F);
    const float beyond = oblique.apply({0.0F, 0.0F, -20.0F}).z;
    EXPECT_GT(beyond, 0.0F);
    EXPECT_LT(beyond, 1.0F);
    EXPECT_TRUE(Vec3::approximately({oblique.apply({0.3F, 0.2F, -20.0F}).x, oblique.apply({0.3F, 0.2F, -20.0F}).y, 0.0F}, {projection.apply({0.3F, 0.2F, -20.0F}).x, projection.apply({0.3F, 0.2F, -20.0F}).y, 0.0F}));

    const Mat4 openGl = Mat4::perspective({.nearPlane = 0.1F, .farPlane = 100.0F, .depthRange = Mat4::DepthRange::NegativeOneToOne, .reversedDepth = false});
    const Mat4 openGlOblique = openGl.getObliqueNearPlane(mirror, Mat4::DepthRange::NegativeOneToOne, false);
    EXPECT_NEAR(openGlOblique.apply(onPlane).z, -1.0F, 1e-4F);

    const Plane facingCamera = Plane::fromPoint({0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -5.0F});
    EXPECT_EQ(errorOf([&] { (void)projection.getObliqueNearPlane(facingCamera, Mat4::DepthRange::ZeroToOne, true); }), "An oblique near plane must face away from the camera, so the camera must lie behind the plane.");
}

TEST_F(MatrixTest, PlanesMeasureSignedDistances) {
    const Plane floor = Plane::fromNormal({0.0F, 2.0F, 0.0F}, 4.0F);
    EXPECT_EQ(floor.normal, Vec3::up());
    EXPECT_EQ(floor.distance, 2.0F);
    EXPECT_EQ(floor.getSignedDistance({3.0F, 5.0F, 1.0F}), 3.0F);
    EXPECT_EQ(floor.getClosestPoint({3.0F, 5.0F, 1.0F}), Vec3(3.0F, 2.0F, 1.0F));
    EXPECT_EQ(floor.getCoefficients(), Vec4(0.0F, 1.0F, 0.0F, -2.0F));

    const Plane ground = Plane::fromPoints({0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 0.0F});
    EXPECT_TRUE(Vec3::approximately(ground.normal, Vec3::up()));
    EXPECT_FLOAT_EQ(ground.distance, 1.0F);
    EXPECT_EQ(errorOf([] { (void)Plane::fromPoints({}, {1.0F, 1.0F, 1.0F}, {2.0F, 2.0F, 2.0F}); }), "The three points lie on one line, so they span no plane.");
    EXPECT_THROW((void)Plane::fromNormal({}, 1.0F), std::invalid_argument);
}

} // namespace haylen::math
