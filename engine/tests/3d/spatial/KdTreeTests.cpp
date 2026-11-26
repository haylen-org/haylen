#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/3d/spatial/KdTree.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Random.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::spatial3d {

// Checks the k-d tree against a search through every sphere.
class KdTree3DTest : public ::testing::Test {
  protected:
    using Ids = std::vector<std::uint64_t>;

    void SetUp() override {
        math::Random random(41);
        for (std::uint64_t id = 0; id < 800; ++id) {
            ids.push_back(id * 5 + 1);
            spheres.push_back({{random.range(-40.0F, 40.0F), random.range(-40.0F, 40.0F), random.range(-40.0F, 40.0F)}, id % 4 == 0 ? 0.0F : random.range(0.0F, 2.0F)});
        }

        tree.setAll(ids, spheres);
        tree.build();
    }

    template <typename Accepts> [[nodiscard]] Ids bruteForce(const Accepts& accepts) const {
        Ids found;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            if (accepts(spheres[index])) {
                found.push_back(ids[index]);
            }
        }

        return found;
    }

    Ids ids;
    std::vector<math::Sphere> spheres;
    KdTree tree;
};

TEST_F(KdTree3DTest, AnswersEveryQueryLikeASearchThroughEverySphere) {
    math::Random random(42);
    Ids found;
    std::vector<RayHit> hits;
    std::vector<Neighbor> neighbors;
    for (int round = 0; round < 80; ++round) {
        const math::Vec3 center{random.range(-40.0F, 40.0F), random.range(-40.0F, 40.0F), random.range(-40.0F, 40.0F)};
        const math::Box area = math::Box::fromCenter(center, {random.range(0.0F, 20.0F), random.range(0.0F, 20.0F), random.range(0.0F, 20.0F)});
        tree.query(area, found);
        EXPECT_EQ(found, bruteForce([&area](const math::Sphere& sphere) { return sphere.intersects(area); })) << round;

        const math::Sphere probe{center, random.range(0.0F, 10.0F)};
        tree.querySphere(probe, found);
        EXPECT_EQ(found, bruteForce([&probe](const math::Sphere& sphere) { return sphere.intersects(probe); })) << round;

        const math::Mat4 view = math::Mat4::lookAt(center * 2.0F, {});
        const math::Frustum frustum = math::Frustum::fromMatrix(math::Mat4::perspective({.nearPlane = 0.5F, .farPlane = random.range(10.0F, 120.0F)}) * view, math::Mat4::DepthRange::ZeroToOne);
        tree.queryFrustum(frustum, found);
        EXPECT_EQ(found, bruteForce([&frustum](const math::Sphere& sphere) { return frustum.classify(sphere) != math::Frustum::Containment::Outside; })) << round;

        const math::Ray3D ray{center * 1.5F, (-center).getNormalized(), std::numeric_limits<float>::infinity()};
        tree.raycast(ray, 0, hits);
        std::vector<float> distances;
        for (const math::Sphere& sphere : spheres) {
            if (const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, sphere)) {
                distances.push_back(hit->distance);
            }
        }

        std::ranges::sort(distances);
        ASSERT_EQ(hits.size(), distances.size()) << round;
        for (std::size_t index = 0; index < hits.size(); ++index) {
            EXPECT_EQ(hits[index].distance, distances[index]) << round;
        }

        tree.nearest(center, 4, std::numeric_limits<float>::infinity(), neighbors);
        std::vector<Neighbor> expected;
        for (std::size_t index = 0; index < ids.size(); ++index) {
            expected.push_back({.id = ids[index], .distance = std::max(0.0F, math::Vec3::distance(center, spheres[index].center) - spheres[index].radius)});
        }

        std::sort(expected.begin(), expected.end());
        expected.resize(4);
        ASSERT_EQ(neighbors.size(), 4U);
        for (std::size_t index = 0; index < 4; ++index) {
            EXPECT_EQ(neighbors[index].id, expected[index].id) << round;
        }
    }
}

TEST_F(KdTree3DTest, NeedsABuildAfterEveryChange) {
    Ids found;
    tree.set(9999, {1.0F, 2.0F, 3.0F}, 0.5F);
    EXPECT_FALSE(tree.isBuilt());
    EXPECT_THROW(tree.queryPoint({}, found), std::logic_error);
    tree.build();
    tree.queryPoint({1.0F, 2.0F, 3.4F}, found);
    EXPECT_TRUE(std::ranges::binary_search(found, std::uint64_t{9999}));
    EXPECT_EQ(tree.getSphere(9999), (math::Sphere{{1.0F, 2.0F, 3.0F}, 0.5F}));
    EXPECT_TRUE(tree.remove(9999));
    EXPECT_FALSE(tree.remove(9999));
    EXPECT_FALSE(tree.getSphere(9999).has_value());

    const std::vector<math::Sphere> refused{{{0.0F, 0.0F, 0.0F}, 1.0F}, {{0.0F, 0.0F, 0.0F}, -1.0F}};
    EXPECT_THROW(tree.setAll(std::vector<std::uint64_t>{1, 2}, refused), std::invalid_argument);
    EXPECT_THROW(tree.set(1, {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}), std::invalid_argument);
    tree.build();
    EXPECT_EQ(tree.size(), ids.size());
}

TEST_F(KdTree3DTest, RepeatsQueriesWithoutAllocating) {
    Ids found;
    std::vector<Neighbor> neighbors;
    tree.querySphere({{}, 20.0F}, found);
    tree.nearest({3.0F, 1.0F, 2.0F}, 8, std::numeric_limits<float>::infinity(), neighbors);
    const test::AllocationTracker tracker;
    tree.querySphere({{}, 20.0F}, found);
    tree.nearest({3.0F, 1.0F, 2.0F}, 8, std::numeric_limits<float>::infinity(), neighbors);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::spatial3d
