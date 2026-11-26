#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Backend.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/spatial/MeshBvh.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics3d {

class PhysicsShape3DTest : public ::testing::Test {
  protected:
    // A backend of another name, which makes the shapes of the backend it wraps, so shapes of two backends meet.
    class OtherBackend final : public Backend {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "other";
        }
        [[nodiscard]] std::unique_ptr<Simulation> createSimulation(const WorldSettings&, core::JobSystem*) override {
            return nullptr;
        }

      protected:
        [[nodiscard]] std::shared_ptr<const Shape> buildShape(const Shape::Settings&) const override {
            return nullptr;
        }
    };

    [[nodiscard]] std::shared_ptr<const Shape> make(Shape::Geometry geometry, float density = 1000.0F) const {
        return backend.createShape({.geometry = std::move(geometry), .density = density});
    }

    // Returns the message of the `std::invalid_argument` that making the shape throws, or an empty text when it throws none.
    [[nodiscard]] std::string refusal(Shape::Geometry geometry) const {
        try {
            static_cast<void>(make(std::move(geometry)));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }

    // The eight corners of a cube of one meter around the origin.
    [[nodiscard]] static std::vector<math::Vec3> cubeCorners() {
        std::vector<math::Vec3> corners;
        for (int corner = 0; corner < 8; ++corner) {
            corners.push_back({(corner & 1) != 0 ? 0.5F : -0.5F, (corner & 2) != 0 ? 0.5F : -0.5F, (corner & 4) != 0 ? 0.5F : -0.5F});
        }

        return corners;
    }

    // A floor of two triangles, ten meters on each side.
    [[nodiscard]] static Shape::Mesh floorMesh() {
        return {.positions = {-5.0F, 0.0F, -5.0F, 5.0F, 0.0F, -5.0F, 5.0F, 0.0F, 5.0F, -5.0F, 0.0F, 5.0F}, .indices = {0, 2, 1, 0, 3, 2}};
    }

    jolt::Backend backend;
};

TEST_F(PhysicsShape3DTest, GivesEachPrimitiveTheVolumeMassAndBoundsOfItsSize) {
    const std::shared_ptr<const Shape> sphere = make(Shape::Sphere{.radius = 0.5F});
    EXPECT_EQ(sphere->getKind(), Shape::Kind::Sphere);
    EXPECT_EQ(sphere->getBackendName(), "jolt");
    EXPECT_NEAR(sphere->getVolume(), 4.0F / 3.0F * math::Math::kPi * 0.125F, 1.0e-4F);
    EXPECT_NEAR(sphere->getMass(), sphere->getVolume() * 1000.0F, 0.1F);
    EXPECT_NEAR(sphere->getBounds().max.y, 0.5F, 1.0e-5F);

    const std::shared_ptr<const Shape> box = make(Shape::Box{.size = {2.0F, 1.0F, 0.5F}}, 500.0F);
    EXPECT_NEAR(box->getVolume(), 1.0F, 1.0e-4F);
    EXPECT_NEAR(box->getMass(), 500.0F, 0.05F);
    EXPECT_NEAR(box->getBounds().min.x, -1.0F, 1.0e-5F);
    EXPECT_NEAR(box->getBounds().max.z, 0.25F, 1.0e-5F);

    const std::shared_ptr<const Shape> wall = make(Shape::Box{.size = {0.02F, 2.0F, 2.0F}});
    EXPECT_NEAR(wall->getBounds().max.x, 0.01F, 1.0e-5F);

    const std::shared_ptr<const Shape> capsule = make(Shape::Capsule{.height = 2.0F, .radius = 0.25F});
    EXPECT_NEAR(capsule->getBounds().max.y, 1.0F, 1.0e-5F);
    EXPECT_NEAR(capsule->getVolume(), math::Math::kPi * 0.0625F * 1.5F + 4.0F / 3.0F * math::Math::kPi * 0.015625F, 1.0e-4F);

    const std::shared_ptr<const Shape> cylinder = make(Shape::Cylinder{.height = 2.0F, .radius = 0.5F});
    EXPECT_NEAR(cylinder->getVolume(), math::Math::kPi * 0.25F * 2.0F, 1.0e-3F);
    EXPECT_NEAR(cylinder->getBounds().max.y, 1.0F, 1.0e-5F);

    const std::shared_ptr<const Shape> taperedCylinder = make(Shape::TaperedCylinder{.height = 1.0F, .topRadius = 0.25F, .bottomRadius = 0.5F});
    EXPECT_NEAR(taperedCylinder->getBounds().max.x, 0.5F, 1.0e-4F);
    EXPECT_LT(taperedCylinder->getCenterOfMass().y, 0.0F);

    const std::shared_ptr<const Shape> taperedCapsule = make(Shape::TaperedCapsule{.height = 2.0F, .topRadius = 0.2F, .bottomRadius = 0.4F});
    EXPECT_NEAR(taperedCapsule->getBounds().max.y - taperedCapsule->getBounds().min.y, 2.0F, 1.0e-4F);
    EXPECT_TRUE(taperedCapsule->isSolid());
}

TEST_F(PhysicsShape3DTest, BuildsTheConvexHullOfItsPoints) {
    std::vector<math::Vec3> points = cubeCorners();
    points.push_back({0.0F, 0.0F, 0.0F});
    points.push_back({0.1F, -0.2F, 0.3F});
    const std::shared_ptr<const Shape> hull = make(Shape::ConvexHull{.points = points, .convexRadius = 0.0F});
    EXPECT_EQ(hull->getKind(), Shape::Kind::ConvexHull);
    EXPECT_NEAR(hull->getVolume(), 1.0F, 1.0e-3F);
    EXPECT_NEAR(hull->getMass(), 1000.0F, 1.0F);
    EXPECT_NEAR(hull->getCenterOfMass().getLength(), 0.0F, 1.0e-4F);
}

TEST_F(PhysicsShape3DTest, BuildsSurfacesWithoutVolumeForStaticBodies) {
    const std::shared_ptr<const Shape> mesh = make(floorMesh());
    EXPECT_FALSE(mesh->isSolid());
    EXPECT_EQ(mesh->getMass(), 0.0F);
    EXPECT_NEAR(mesh->getBounds().max.x, 5.0F, 1.0e-4F);

    const spatial3d::MeshBvh bvh(floorMesh().positions, floorMesh().indices);
    EXPECT_NEAR(make(Shape::Mesh::fromBvh(bvh))->getBounds().min.z, -5.0F, 1.0e-4F);

    std::vector<float> heights(16, 0.0F);
    heights[5] = 2.0F;
    const std::shared_ptr<const Shape> field = make(Shape::HeightField{.heights = heights, .sampleCount = 4, .scale = {1.0F, 1.0F, 1.0F}, .holes = {15}});
    EXPECT_EQ(field->getKind(), Shape::Kind::HeightField);
    EXPECT_FALSE(field->isSolid());
    EXPECT_NEAR(field->getBounds().max.y, 2.0F, 0.05F);

    const std::shared_ptr<const Shape> plane = make(Shape::Plane{.normal = {0.0F, 2.0F, 0.0F}, .distance = 1.0F, .halfExtent = 50.0F});
    EXPECT_FALSE(plane->isSolid());
    EXPECT_NEAR(plane->getBounds().max.x, 50.0F, 1.0e-3F);
}

TEST_F(PhysicsShape3DTest, CombinesScalesAndOffsetsShapes) {
    const std::shared_ptr<const Shape> box = make(Shape::Box{.size = {1.0F, 1.0F, 1.0F}});
    const std::shared_ptr<const Shape> ball = make(Shape::Sphere{.radius = 0.5F});
    const std::shared_ptr<const Shape> compound = make(Shape::StaticCompound{.children = {{.shape = box, .position = {-2.0F, 0.0F, 0.0F}}, {.shape = box, .position = {2.0F, 0.0F, 0.0F}}}});
    EXPECT_EQ(compound->getKind(), Shape::Kind::StaticCompound);
    EXPECT_NEAR(compound->getMass(), 2.0F * box->getMass(), 0.5F);
    EXPECT_NEAR(compound->getBounds().max.x, 2.5F, 1.0e-4F);
    EXPECT_NEAR(compound->getCenterOfMass().x, 0.0F, 1.0e-4F);

    const std::shared_ptr<const Shape> mutableCompound = make(Shape::MutableCompound{.children = {{.shape = ball, .position = {0.0F, 1.0F, 0.0F}}}});
    EXPECT_EQ(mutableCompound->getKind(), Shape::Kind::MutableCompound);
    EXPECT_NEAR(mutableCompound->getCenterOfMass().y, 1.0F, 1.0e-4F);

    const std::shared_ptr<const Shape> scaled = make(Shape::Scaled{.shape = box, .scale = {2.0F, 3.0F, 4.0F}});
    EXPECT_NEAR(scaled->getVolume(), 24.0F * box->getVolume(), 1.0e-2F);

    const std::shared_ptr<const Shape> offset = make(Shape::OffsetCenterOfMass{.shape = box, .offset = {0.0F, -0.4F, 0.0F}});
    EXPECT_NEAR(offset->getCenterOfMass().y, -0.4F, 1.0e-5F);
    EXPECT_NEAR(offset->getMass(), box->getMass(), 1.0e-2F);

    const std::shared_ptr<const Shape> floor = make(floorMesh());
    EXPECT_FALSE(make(Shape::StaticCompound{.children = {{.shape = box}, {.shape = floor, .position = {0.0F, -1.0F, 0.0F}}}})->isSolid());
}

TEST_F(PhysicsShape3DTest, RefusesShapesThatHoldNoSolidWithASentence) {
    EXPECT_EQ(refusal(Shape::Sphere{.radius = 0.0F}), "The radius of a sphere must be a finite number above zero.");
    EXPECT_EQ(refusal(Shape::Box{.size = {1.0F, 1.0F, 1.0F}, .convexRadius = 0.6F}), "The convex radius of a box must be from 0 to 0.5, half of its smallest size.");
    EXPECT_EQ(refusal(Shape::Capsule{.height = 1.0F, .radius = 0.5F}), "The height of a capsule, from the bottom of one cap to the top of the other, must be more than twice its radius. Make a sphere for a shorter one.");
    EXPECT_EQ(refusal(Shape::TaperedCapsule{.height = 1.0F, .topRadius = 0.05F, .bottomRadius = 0.5F}), "The caps of a tapered capsule must not hold each other. Make it taller than the sum of its radii plus their difference, or make a sphere.");
    EXPECT_EQ(refusal(Shape::ConvexHull{.points = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 1.0F}, {0.5F, 0.0F, 0.5F}}}), "The points of a convex hull lie on one plane or one line, so they hold no volume. Give it points that spread along every axis.");
    EXPECT_EQ(refusal(Shape::ConvexHull{.points = {{0.0F, 0.0F, 0.0F}}}), "A convex hull needs from 4 to 65536 points.");
}

TEST_F(PhysicsShape3DTest, RefusesMalformedMeshesWithASentence) {
    EXPECT_EQ(refusal(Shape::Mesh{.positions = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}, .indices = {0, 1, 3}}), "A triangle of a triangle mesh names the vertex 3, and the mesh has 3 vertices.");
    EXPECT_EQ(refusal(Shape::Mesh{.positions = {0.0F, 0.0F}}), "A triangle mesh takes three numbers for each vertex, for up to 16777216 vertices.");
    EXPECT_EQ(refusal(Shape::Mesh{.positions = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F}}), "A triangle mesh needs at least one triangle with an area, and every triangle of this one has its corners on one line.");
    EXPECT_EQ(refusal(Shape::Mesh{.positions = {0.0F, NAN, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}}), "The positions of a triangle mesh must be finite numbers.");
    EXPECT_EQ(refusal(Shape::Mesh{}), "A triangle mesh needs at least one triangle, with three vertices for each triangle.");
}

TEST_F(PhysicsShape3DTest, RefusesHeightFieldsWhoseHeightsDoNotFillTheirGrid) {
    EXPECT_EQ(refusal(Shape::HeightField{.heights = std::vector<float>(15, 0.0F), .sampleCount = 4}), "A heightfield of 4 samples on each side needs 16 heights, and it has 15.");
    EXPECT_EQ(refusal(Shape::HeightField{.heights = std::vector<float>(4, 0.0F), .sampleCount = 2}), "A heightfield needs from 3 to 4096 samples on each side.");
    EXPECT_EQ(refusal(Shape::HeightField{.heights = std::vector<float>(9, 0.0F), .sampleCount = 3, .holes = {9}}), "A hole of a heightfield names the sample 9, and the heightfield has 9 samples.");
    EXPECT_EQ(refusal(Shape::HeightField{.heights = std::vector<float>(9, 0.0F), .sampleCount = 3, .scale = {1.0F, 0.0F, 1.0F}}), "The scale of a heightfield along y must be a finite number above zero.");
}

TEST_F(PhysicsShape3DTest, RefusesCompoundsAndScalesThatDoNotSuitTheirShapes) {
    const std::shared_ptr<const Shape> ball = make(Shape::Sphere{.radius = 0.5F});
    const std::shared_ptr<const Shape> plane = make(Shape::Plane{});
    EXPECT_EQ(refusal(Shape::StaticCompound{}), "A static compound needs at least one child.");
    EXPECT_EQ(refusal(Shape::StaticCompound{.children = {{.shape = plane}}}), "A plane stands alone and is never the child of a static compound. Give the plane a body of its own.");
    EXPECT_EQ(refusal(Shape::StaticCompound{.children = {{.shape = ball, .rotation = {0.0F, 0.0F, 0.0F, 2.0F}}}}), "The rotation of a child of a static compound must be a rotation, a quaternion of length one.");
    EXPECT_EQ(refusal(Shape::Scaled{.shape = ball, .scale = {1.0F, 2.0F, 1.0F}}), "The scale does not suit the shape it scales. Spheres and capsules take the same scale on every axis, cylinders the same scale on x and z, and the children of a compound the scales their own shapes take.");
    EXPECT_EQ(refusal(Shape::Scaled{.shape = ball, .scale = {1.0F, 0.0F, 1.0F}}), "The scale of a scaled shape must not be zero on any axis.");

    const OtherBackend other;
    const Shape::Settings foreign{.geometry = Shape::Scaled{.shape = ball, .scale = {2.0F, 2.0F, 2.0F}}};
    try {
        static_cast<void>(other.createShape(foreign));
        FAIL() << "A shape of another backend joined a shape.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The shape of a scaled shape was made by the 3D physics backend \"jolt\", and a shape takes only shapes of its own backend \"other\".");
    }
}

} // namespace haylen::physics3d
