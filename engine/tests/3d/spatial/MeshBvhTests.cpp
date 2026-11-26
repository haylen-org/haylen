#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/3d/spatial/MeshBvh.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "haylen/math/Random.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::spatial3d {

// Checks the hierarchy of a mesh against a search through every triangle, on a soup of random triangles and on a grid of shared edges.
class MeshBvhTest : public ::testing::Test {
  protected:
    static constexpr int kGridSide = 24;

    // Returns the positions of random triangles, three vertices each.
    [[nodiscard]] static std::vector<float> randomSoup(math::Random& random, std::size_t count) {
        std::vector<float> positions;
        for (std::size_t triangle = 0; triangle < count; ++triangle) {
            const math::Vec3 center{random.range(-30.0F, 30.0F), random.range(-30.0F, 30.0F), random.range(-30.0F, 30.0F)};
            for (int corner = 0; corner < 3; ++corner) {
                positions.insert(positions.end(), {center.x + random.range(-3.0F, 3.0F), center.y + random.range(-3.0F, 3.0F), center.z + random.range(-3.0F, 3.0F)});
            }
        }

        return positions;
    }

    // Returns a flat grid on the plane of y zero, whose squares share their edges, as a mesh of indexed vertices.
    [[nodiscard]] static MeshBvh grid() {
        std::vector<float> positions;
        for (int z = 0; z <= kGridSide; ++z) {
            for (int x = 0; x <= kGridSide; ++x) {
                positions.insert(positions.end(), {static_cast<float>(x), 0.0F, static_cast<float>(-z)});
            }
        }

        std::vector<std::uint32_t> indices;
        const auto vertex = [](int x, int z) { return static_cast<std::uint32_t>(z * (kGridSide + 1) + x); };
        for (int z = 0; z < kGridSide; ++z) {
            for (int x = 0; x < kGridSide; ++x) {
                indices.insert(indices.end(), {vertex(x, z), vertex(x + 1, z), vertex(x + 1, z + 1), vertex(x, z), vertex(x + 1, z + 1), vertex(x, z + 1)});
            }
        }

        return MeshBvh(std::move(positions), std::move(indices));
    }

    [[nodiscard]] static std::string errorOf(const auto& call) {
        try {
            call();
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }
};

TEST_F(MeshBvhTest, CastsRaysAndFindsPointsLikeASearchThroughEveryTriangle) {
    math::Random random(61);
    const MeshBvh mesh(randomSoup(random, 1500));
    ASSERT_EQ(mesh.getTriangleCount(), 1500U);
    EXPECT_EQ(mesh.getVertexCount(), 4500U);
    for (int round = 0; round < 150; ++round) {
        const math::Vec3 origin{random.range(-50.0F, 50.0F), random.range(-50.0F, 50.0F), random.range(-50.0F, 50.0F)};
        const math::Vec3 target{random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F), random.range(-20.0F, 20.0F)};
        const math::Ray3D ray{origin, (target - origin).getNormalized(), round % 2 == 0 ? std::numeric_limits<float>::infinity() : 40.0F};

        std::optional<MeshBvh::Hit> expected;
        for (std::uint32_t triangle = 0; triangle < mesh.getTriangleCount(); ++triangle) {
            const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, mesh.getTriangle(triangle));
            if (hit && (!expected || hit->distance < expected->distance)) {
                expected = MeshBvh::Hit{.triangle = triangle, .point = hit->point, .normal = hit->normal, .barycentric = {}, .distance = hit->distance, .front = true};
            }
        }

        const std::optional<MeshBvh::Hit> hit = mesh.raycast(ray);
        ASSERT_EQ(hit.has_value(), expected.has_value()) << round;
        EXPECT_EQ(mesh.intersects(ray), expected.has_value()) << round;
        if (hit) {
            EXPECT_EQ(hit->triangle, expected->triangle) << round;
            EXPECT_EQ(hit->distance, expected->distance) << round;
            EXPECT_EQ(hit->normal, expected->normal) << round;

            // The weights of the corners put the point back together.
            const math::Triangle corners = mesh.getTriangle(hit->triangle);
            const math::Vec3 rebuilt = corners.a * hit->barycentric.x + corners.b * hit->barycentric.y + corners.c * hit->barycentric.z;
            EXPECT_LT(math::Vec3::distance(rebuilt, hit->point), 1e-3F) << round;
            EXPECT_NEAR(hit->barycentric.x + hit->barycentric.y + hit->barycentric.z, 1.0F, 1e-5F) << round;
            EXPECT_EQ(hit->front, math::Vec3::dot(corners.getNormal(), ray.direction) < 0.0F) << round;
        }

        float nearest = std::numeric_limits<float>::infinity();
        for (std::uint32_t triangle = 0; triangle < mesh.getTriangleCount(); ++triangle) {
            nearest = std::min(nearest, math::Vec3::distance(mesh.getTriangle(triangle).getClosestPoint(origin), origin));
        }

        const std::optional<MeshBvh::ClosestPoint> closest = mesh.closestPoint(origin);
        ASSERT_TRUE(closest.has_value());
        EXPECT_EQ(closest->distance, nearest) << round;
        EXPECT_FALSE(mesh.closestPoint(origin, nearest * 0.5F).has_value() && nearest > 0.0F) << round;

        const math::Sphere sphere{target, random.range(0.0F, 6.0F)};
        std::vector<std::uint32_t> found;
        mesh.querySphere(sphere, found);
        std::vector<std::uint32_t> touching;
        for (std::uint32_t triangle = 0; triangle < mesh.getTriangleCount(); ++triangle) {
            if (sphere.contains(mesh.getTriangle(triangle).getClosestPoint(sphere.center))) {
                touching.push_back(triangle);
            }
        }

        EXPECT_EQ(found, touching) << round;
    }
}

TEST_F(MeshBvhTest, FindsTheTrianglesInABoxBySeparatingAxes) {
    math::Random random(62);
    const MeshBvh mesh(randomSoup(random, 800));
    std::vector<std::uint32_t> found;
    for (int round = 0; round < 60; ++round) {
        const math::Box area = math::Box::fromCenter({random.range(-30.0F, 30.0F), random.range(-30.0F, 30.0F), random.range(-30.0F, 30.0F)}, {random.range(0.0F, 12.0F), random.range(0.0F, 12.0F), random.range(0.0F, 12.0F)});
        mesh.queryBox(area, found);
        for (std::uint32_t triangle = 0; triangle < mesh.getTriangleCount(); ++triangle) {
            const math::Triangle corners = mesh.getTriangle(triangle);
            const bool listed = std::ranges::binary_search(found, triangle);
            const math::Box bounds = math::Box{corners.a, corners.a}.merged(corners.b).merged(corners.c);

            // A corner inside the box makes the triangle touch it, and a triangle whose bounds miss the box cannot.
            if (area.contains(corners.a) || area.contains(corners.b) || area.contains(corners.c)) {
                EXPECT_TRUE(listed) << round;
            }

            if (!bounds.intersects(area)) {
                EXPECT_FALSE(listed) << round;
            }
        }
    }

    // The triangle crosses the box with every corner outside it, and the other one passes by a corner of the box inside its own bounds.
    const MeshBvh crossing({-5.0F, 0.5F, 0.5F, 5.0F, 0.5F, 0.5F, 0.0F, 6.0F, 0.5F, 3.5F, 0.0F, 0.0F, 0.0F, 3.5F, 0.0F, 0.0F, 0.0F, 3.5F});
    crossing.queryBox({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, found);
    EXPECT_EQ(found, (std::vector<std::uint32_t>{0}));
}

TEST_F(MeshBvhTest, LetsNoRaySlipThroughTheSharedEdgesOfAGrid) {
    const MeshBvh mesh = grid();
    ASSERT_EQ(mesh.getTriangleCount(), static_cast<std::size_t>(kGridSide * kGridSide * 2));
    for (int x = 1; x < kGridSide * 4; ++x) {
        for (int z = 1; z < kGridSide * 4; ++z) {
            const math::Vec3 above{static_cast<float>(x) * 0.25F, 5.0F, -static_cast<float>(z) * 0.25F};
            const std::optional<MeshBvh::Hit> hit = mesh.raycast({above, -math::Vec3::up(), 10.0F});
            ASSERT_TRUE(hit.has_value()) << x << " " << z;
            EXPECT_EQ(hit->distance, 5.0F);
            EXPECT_EQ(hit->normal, math::Vec3::up());
            EXPECT_TRUE(hit->front);
        }
    }

    const std::optional<MeshBvh::Hit> below = mesh.raycast({{3.3F, -2.0F, -4.4F}, math::Vec3::up()});
    ASSERT_TRUE(below.has_value());
    EXPECT_FALSE(below->front);
    EXPECT_EQ(below->normal, -math::Vec3::up());
    EXPECT_FALSE(mesh.intersects({{3.3F, 2.0F, -4.4F}, math::Vec3::up()}));
}

TEST_F(MeshBvhTest, RefusesMeshesThatDoNotMakeWholeTriangles) {
    EXPECT_EQ(errorOf([] { MeshBvh({0.0F, 1.0F}); }), "The positions of a mesh need three numbers for each vertex.");
    EXPECT_EQ(errorOf([] { MeshBvh({0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}); }), "A mesh without indices needs three vertices for each triangle.");
    EXPECT_EQ(errorOf([] { MeshBvh({0.0F, 0.0F, std::numeric_limits<float>::quiet_NaN()}, {0, 0, 0}); }), "Every position of a mesh must be a finite number.");
    EXPECT_EQ(errorOf([] { MeshBvh({0.0F, 0.0F, 0.0F}, {0, 0}); }), "The indices of a mesh need three vertices for each triangle.");
    EXPECT_EQ(errorOf([] { MeshBvh({0.0F, 0.0F, 0.0F}, {0, 0, 1}); }), "Index 2 of the mesh names vertex 1, but the vertex count of the mesh is 1.");

    const MeshBvh empty({});
    EXPECT_EQ(empty.getTriangleCount(), 0U);
    EXPECT_FALSE(empty.getBounds().has_value());
    EXPECT_FALSE(empty.raycast({{}, math::Vec3::forward()}).has_value());
    EXPECT_FALSE(empty.closestPoint({}).has_value());
    EXPECT_EQ(errorOf([&empty] { (void)empty.getTriangle(0); }), "The triangle count of the mesh is 0, so it has no triangle 0.");
}

TEST_F(MeshBvhTest, CastsAndMeasuresWithoutAllocating) {
    const MeshBvh mesh = grid();
    std::vector<std::uint32_t> found;
    found.reserve(mesh.getTriangleCount());
    const test::AllocationTracker tracker;
    for (int round = 0; round < 10; ++round) {
        const float place = static_cast<float>(round) + 0.5F;
        (void)mesh.raycast({{place, 3.0F, -place}, -math::Vec3::up()});
        (void)mesh.intersects({{place, 3.0F, -place}, -math::Vec3::up()});
        (void)mesh.closestPoint({place, 1.0F, -place});
        mesh.querySphere({{place, 0.0F, -place}, 2.0F}, found);
        mesh.queryBox({{place, -1.0F, -place - 2.0F}, {place + 2.0F, 1.0F, -place}}, found);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(mesh.getBounds(), (math::Box{{0.0F, 0.0F, -static_cast<float>(kGridSide)}, {static_cast<float>(kGridSide), 0.0F, 0.0F}}));
}

} // namespace haylen::spatial3d
