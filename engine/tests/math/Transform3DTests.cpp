#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/math/DualQuat.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Transform3D.hpp"

namespace haylen::math {

class Transform3DTest : public ::testing::Test {
  protected:
    static std::string errorOf(const auto& call) {
        try {
            call();
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }
};

TEST_F(Transform3DTest, PlacesAChildInTheSpaceOfItsParent) {
    const Transform3D parent{{1.0F, 2.0F, 3.0F}, Quat::identity(), {1.0F, 1.0F, 1.0F}};
    const Transform3D child{{4.0F, 0.0F, 0.0F}, Quat::identity(), {1.0F, 1.0F, 1.0F}};
    const Transform3D world = parent * child;
    EXPECT_EQ(world.position, Vec3(5.0F, 2.0F, 3.0F));
    EXPECT_EQ(world.apply({}), Vec3(5.0F, 2.0F, 3.0F));
    EXPECT_EQ(Transform3D::identity() * child, child);
}

TEST_F(Transform3DTest, ChainsADeepHierarchyLikeItsMatrices) {
    const std::array<Transform3D, 6> chain{{
        {{1.0F, 2.0F, 3.0F}, Quat::fromAxisAngle(Vec3::up(), 0.7F), {2.0F, 2.0F, 2.0F}},
        {{-3.0F, 0.0F, 1.0F}, Quat::fromAxisAngle({1.0F, 0.5F, 0.0F}, 2.1F), {-1.0F, -1.0F, -1.0F}},
        {{0.5F, 4.0F, 0.0F}, Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, -1.2F), {0.5F, 0.5F, 0.5F}},
        {{2.0F, -1.0F, -2.0F}, Quat::identity(), {1.0F, -2.0F, 3.0F}},
        {{0.0F, 0.0F, 7.0F}, Quat::identity(), {3.0F, 1.0F, -1.0F}},
        {{1.0F, 1.0F, 1.0F}, Quat::identity(), {1.0F, 2.0F, 3.0F}},
    }};

    Transform3D world;
    Mat4 matrix;
    for (const Transform3D& link : chain) {
        world = world * link;
        matrix = matrix * link.toMatrix();
    }

    for (const Vec3 point : {Vec3{}, Vec3{1.0F, -2.0F, 0.5F}, Vec3{-4.0F, 3.0F, 9.0F}}) {
        EXPECT_TRUE(Vec3::approximately(world.apply(point), matrix.apply(point), 1e-4F));
        EXPECT_TRUE(Vec3::approximately(world.applyVector(point), matrix.applyVector(point), 1e-4F));
        EXPECT_TRUE(Vec3::approximately(world.applyInverse(world.apply(point)), point, 1e-4F));
    }

    EXPECT_LT(matrix.getDeterminant(), 0.0F);
    EXPECT_TRUE(Vec3::approximately(world.applyNormal({0.0F, 1.0F, 0.0F}), (matrix.getNormalMatrix() * Vec3{0.0F, 1.0F, 0.0F}).getNormalized(), 1e-4F));
}

TEST_F(Transform3DTest, KeepsNormalsAtRightAnglesUnderUnevenScale) {
    const Transform3D squash{{}, Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, 0.3F), {4.0F, 1.0F, 1.0F}};
    const Vec3 tangent{1.0F, 1.0F, 0.0F};
    const Vec3 normal{1.0F, -1.0F, 0.0F};
    EXPECT_NEAR(Vec3::dot(squash.applyVector(tangent), squash.applyNormal(normal)), 0.0F, 1e-5F);
    EXPECT_NEAR(squash.applyNormal(normal).getLength(), 1.0F, 1e-6F);
}

TEST_F(Transform3DTest, InvertsUniformScaleAndRefusesTheRest) {
    const Transform3D placement{{3.0F, -1.0F, 2.0F}, Quat::fromAxisAngle({1.0F, 1.0F, 0.0F}, 0.9F), {2.0F, 2.0F, 2.0F}};
    EXPECT_TRUE(Transform3D::approximately(placement * placement.getInverse(), Transform3D::identity(), 1e-5F));
    EXPECT_TRUE(Vec3::approximately(placement.getInverse().apply(placement.apply({1.0F, 2.0F, 3.0F})), {1.0F, 2.0F, 3.0F}, 1e-5F));

    const Transform3D uneven{{}, Quat::identity(), {1.0F, 2.0F, 1.0F}};
    EXPECT_EQ(errorOf([&] { (void)uneven.getInverse(); }), "A transform whose scale differs between axes has no inverse of the same form. Invert its matrix instead.");
    const Transform3D flat{{}, Quat::identity(), {1.0F, 0.0F, 1.0F}};
    EXPECT_EQ(errorOf([&] { (void)flat.applyInverse({1.0F, 1.0F, 1.0F}); }), "The transform scales an axis to zero or to infinity, so nothing undoes it.");
    EXPECT_THROW((void)flat.applyNormal(Vec3::up()), std::invalid_argument);
}

TEST_F(Transform3DTest, ConvertsToAndFromMatricesAndInterpolates) {
    const Transform3D placement{{1.0F, 2.0F, 3.0F}, Quat::fromAxisAngle(Vec3::up(), 0.7F), {-1.0F, 2.0F, 3.0F}};
    EXPECT_TRUE(Transform3D::approximately(Transform3D::fromMatrix(placement.toMatrix()), placement, 1e-5F));

    const Transform3D start{{}, Quat::identity(), {1.0F, 1.0F, 1.0F}};
    const Transform3D end{{10.0F, 0.0F, 0.0F}, Quat::fromAxisAngle(Vec3::up(), Math::kHalfPi), {3.0F, 3.0F, 3.0F}};
    const Transform3D middle = Transform3D::lerp(start, end, 0.5F);
    EXPECT_EQ(middle.position, Vec3(5.0F, 0.0F, 0.0F));
    EXPECT_EQ(middle.scale, Vec3(2.0F, 2.0F, 2.0F));
    EXPECT_NEAR(middle.rotation.getAngle(), Math::kPi / 4.0F, 1e-5F);
    EXPECT_EQ(Transform3D::lerp(start, end, 1.0F), end);
}

TEST_F(Transform3DTest, DualQuaternionsPlaceAndBlendRigidJoints) {
    const Quat turn = Quat::fromAxisAngle(Vec3::up(), Math::kHalfPi);
    const DualQuat joint = DualQuat::fromRotationTranslation(turn, {1.0F, 2.0F, 3.0F});
    EXPECT_TRUE(Vec3::approximately(joint.getTranslation(), {1.0F, 2.0F, 3.0F}));
    EXPECT_TRUE(Vec3::approximately(joint.apply(Vec3::right()), Vec3{1.0F, 2.0F, 3.0F} + Vec3::forward()));
    EXPECT_TRUE(Mat4::approximately(joint.toMatrix(), Mat4::compose({1.0F, 2.0F, 3.0F}, turn, {1.0F, 1.0F, 1.0F})));

    const DualQuat lift = DualQuat::fromRotationTranslation(Quat::identity(), {0.0F, 5.0F, 0.0F});
    EXPECT_TRUE(Vec3::approximately((lift * joint).apply(Vec3::right()), joint.apply(Vec3::right()) + Vec3{0.0F, 5.0F, 0.0F}));

    // Blending two joints half a turn apart around the bone keeps a point at the distance it had from the bone, which blending matrices shrinks toward the bone.
    const DualQuat straight = DualQuat::identity();
    const DualQuat twisted = DualQuat::fromRotationTranslation(Quat::fromAxisAngle(Vec3::right(), Math::kPi * 0.9F), {});
    const std::array<DualQuat, 2> joints{straight, twisted};
    const std::array<float, 2> weights{0.5F, 0.5F};
    const DualQuat blended = DualQuat::blend(joints, weights);
    EXPECT_NEAR(blended.apply({0.0F, 1.0F, 0.0F}).getLength(), 1.0F, 1e-5F);
    EXPECT_NEAR(blended.real.getAngle(), Math::kPi * 0.45F, 1e-5F);

    const DualQuat opposite{{-twisted.real.x, -twisted.real.y, -twisted.real.z, -twisted.real.w}, {-twisted.dual.x, -twisted.dual.y, -twisted.dual.z, -twisted.dual.w}};
    const std::array<DualQuat, 2> flipped{straight, opposite};
    EXPECT_TRUE(DualQuat::approximately(DualQuat::blend(flipped, weights), blended));

    EXPECT_EQ(errorOf([&] { (void)DualQuat::blend(joints, std::array<float, 1>{1.0F}); }), "A blend of dual quaternions needs one weight for each dual quaternion.");
    EXPECT_EQ(errorOf([] { (void)DualQuat::blend({}, {}); }), "A blend of dual quaternions needs at least one dual quaternion.");
    EXPECT_EQ(errorOf([&] { (void)DualQuat::blend(joints, std::array<float, 2>{0.0F, 0.0F}); }), "The weights of a blend of dual quaternions add up to zero, so they blend nothing.");
    EXPECT_EQ(errorOf([&] { (void)DualQuat::blend(joints, std::array<float, 2>{-1.0F, 1.0F}); }), "The weights of a blend of dual quaternions must be finite and not negative.");
}

} // namespace haylen::math
