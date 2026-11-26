#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "haylen/3d/spatial/AabbTree.hpp"
#include "haylen/3d/spatial/Bvh.hpp"
#include "haylen/3d/spatial/HashGrid.hpp"
#include "haylen/3d/spatial/Octree.hpp"
#include "haylen/math/Frustum.hpp"
#include "haylen/math/Geometry3D.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::spatial3d {

// Checks every structure of boxes against a search through every entry, on seeded random scenes.
template <typename Structure> class Spatial3DTest : public ::testing::Test {
  protected:
    using Ids = std::vector<std::uint64_t>;

    struct Scene {
        Ids ids;
        std::vector<math::Box> boxes;
    };

    static constexpr bool kBuilds = std::is_same_v<Structure, Bvh>;

    [[nodiscard]] static Structure make() {
        if constexpr (std::is_same_v<Structure, HashGrid>) {
            return HashGrid(4.0F);
        } else if constexpr (std::is_same_v<Structure, Octree>) {
            return Octree({{-60.0F, -60.0F, -60.0F}, {60.0F, 60.0F, 60.0F}}, {.maxEntries = 4, .maxDepth = 6});
        } else {
            return Structure();
        }
    }

    [[nodiscard]] static Structure makeForHugeBoxes() {
        if constexpr (std::is_same_v<Structure, HashGrid>) {
            return HashGrid(1e38F);
        } else {
            return make();
        }
    }

    static void ready(Structure& structure) {
        if constexpr (kBuilds) {
            structure.build();
        }
    }

    [[nodiscard]] static math::Vec3 randomPoint(math::Random& random, float reach) {
        return {random.range(-reach, reach), random.range(-reach, reach), random.range(-reach, reach)};
    }

    // Some entries have no size, so the structures must find points as well as boxes, and a few lie outside the region of the octree.
    [[nodiscard]] static Scene randomScene(math::Random& random, std::size_t count) {
        Scene scene;
        for (std::size_t index = 0; index < count; ++index) {
            const math::Vec3 center = randomPoint(random, index % 50 == 0 ? 70.0F : 50.0F);
            const math::Vec3 size = index % 7 == 0 ? math::Vec3{} : math::Vec3{random.range(0.0F, 4.0F), random.range(0.0F, 4.0F), random.range(0.0F, 4.0F)};
            scene.ids.push_back(index * 3 + 7);
            scene.boxes.push_back(math::Box::fromCenter(center, size));
        }

        return scene;
    }

    template <typename Accepts> [[nodiscard]] static Ids bruteForce(const Scene& scene, const Accepts& accepts) {
        Ids ids;
        for (std::size_t index = 0; index < scene.ids.size(); ++index) {
            if (accepts(scene.boxes[index])) {
                ids.push_back(scene.ids[index]);
            }
        }

        std::ranges::sort(ids);
        return ids;
    }

    [[nodiscard]] static std::vector<RayHit> bruteForceHits(const Scene& scene, const math::Ray3D& ray, std::size_t limit) {
        std::vector<RayHit> hits;
        for (std::size_t index = 0; index < scene.ids.size(); ++index) {
            if (const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, scene.boxes[index])) {
                hits.push_back({.id = scene.ids[index], .point = hit->point, .normal = hit->normal, .distance = hit->distance});
            }
        }

        std::ranges::sort(hits, [](const RayHit& lhs, const RayHit& rhs) { return lhs.distance != rhs.distance ? lhs.distance < rhs.distance : lhs.id < rhs.id; });
        if (limit > 0 && hits.size() > limit) {
            hits.resize(limit);
        }

        return hits;
    }

    [[nodiscard]] static std::vector<Neighbor> bruteForceNearest(const Scene& scene, math::Vec3 point, std::size_t count, float maxDistance) {
        std::vector<Neighbor> neighbors;
        for (std::size_t index = 0; index < scene.ids.size(); ++index) {
            const float distance = math::Vec3::distance(scene.boxes[index].clamp(point), point);
            if (distance <= maxDistance) {
                neighbors.push_back({.id = scene.ids[index], .distance = distance});
            }
        }

        std::sort(neighbors.begin(), neighbors.end());
        if (neighbors.size() > count) {
            neighbors.resize(count);
        }

        return neighbors;
    }

    [[nodiscard]] static math::Frustum randomFrustum(math::Random& random) {
        const math::Vec3 eye = randomPoint(random, 80.0F);
        const math::Vec3 target = randomPoint(random, 20.0F);
        const float farPlane = random.range(0.0F, 1.0F) < 0.3F ? std::numeric_limits<float>::infinity() : random.range(20.0F, 150.0F);
        const math::Mat4 projection = math::Mat4::perspective({.fieldOfView = math::Math::radians(random.range(30.0F, 90.0F)), .aspect = random.range(0.5F, 2.0F), .nearPlane = 0.5F, .farPlane = farPlane});
        return math::Frustum::fromMatrix(projection * math::Mat4::lookAt(eye, target), math::Mat4::DepthRange::ZeroToOne);
    }

    [[nodiscard]] static math::Ray3D randomRay(math::Random& random) {
        const math::Vec3 origin = randomPoint(random, 80.0F);
        const math::Vec3 direction = (randomPoint(random, 30.0F) - origin).getNormalized();
        return {origin, direction, random.range(0.0F, 1.0F) < 0.5F ? std::numeric_limits<float>::infinity() : random.range(10.0F, 200.0F)};
    }

    static void expectSameHits(const std::vector<RayHit>& actual, const std::vector<RayHit>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < actual.size(); ++index) {
            EXPECT_EQ(actual[index].id, expected[index].id) << index;
            EXPECT_EQ(actual[index].distance, expected[index].distance) << index;
            EXPECT_EQ(actual[index].normal, expected[index].normal) << index;
        }
    }

    static void expectSameNeighbors(const std::vector<Neighbor>& actual, const std::vector<Neighbor>& expected) {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t index = 0; index < actual.size(); ++index) {
            EXPECT_EQ(actual[index].id, expected[index].id) << index;
            EXPECT_EQ(actual[index].distance, expected[index].distance) << index;
        }
    }

    // Runs every query of the structure against the search through every entry.
    static void expectMatchesBruteForce(const Structure& structure, const Scene& scene, math::Random& random) {
        Ids ids;
        std::vector<RayHit> hits;
        std::vector<Neighbor> neighbors;
        for (int round = 0; round < 60; ++round) {
            const math::Box area = math::Box::fromCenter(randomPoint(random, 50.0F), {random.range(0.0F, 30.0F), random.range(0.0F, 30.0F), random.range(0.0F, 30.0F)});
            structure.query(area, ids);
            EXPECT_EQ(ids, bruteForce(scene, [&area](const math::Box& box) { return box.intersects(area); })) << round;

            const math::Sphere sphere{randomPoint(random, 50.0F), random.range(0.0F, 20.0F)};
            structure.querySphere(sphere, ids);
            EXPECT_EQ(ids, bruteForce(scene, [&sphere](const math::Box& box) { return sphere.intersects(box); })) << round;

            const math::Vec3 point = scene.boxes[static_cast<std::size_t>(round) % scene.boxes.size()].getCenter();
            structure.queryPoint(point, ids);
            EXPECT_EQ(ids, bruteForce(scene, [point](const math::Box& box) { return box.contains(point); })) << round;

            const math::Frustum frustum = randomFrustum(random);
            structure.queryFrustum(frustum, ids);
            EXPECT_EQ(ids, bruteForce(scene, [&frustum](const math::Box& box) { return frustum.classify(box) != math::Frustum::Containment::Outside; })) << round;

            const math::Ray3D ray = randomRay(random);
            const std::size_t limit = static_cast<std::size_t>(round % 3) * 2;
            structure.raycast(ray, limit, hits);
            expectSameHits(hits, bruteForceHits(scene, ray, limit));

            const std::size_t count = static_cast<std::size_t>(round % 6);
            const float maxDistance = round % 4 == 0 ? std::numeric_limits<float>::infinity() : random.range(0.0F, 25.0F);
            structure.nearest(point + math::Vec3{1.5F, -0.5F, 2.0F}, count, maxDistance, neighbors);
            expectSameNeighbors(neighbors, bruteForceNearest(scene, point + math::Vec3{1.5F, -0.5F, 2.0F}, count, maxDistance));
        }
    }
};

using Structures = ::testing::Types<HashGrid, Octree, AabbTree, Bvh>;
TYPED_TEST_SUITE(Spatial3DTest, Structures);

TYPED_TEST(Spatial3DTest, AnswersEveryQueryLikeASearchThroughEveryEntry) {
    math::Random random(31);
    const typename TestFixture::Scene scene = TestFixture::randomScene(random, 700);
    TypeParam structure = TestFixture::make();
    structure.setAll(scene.ids, scene.boxes);
    TestFixture::ready(structure);
    EXPECT_EQ(structure.size(), scene.ids.size());
    TestFixture::expectMatchesBruteForce(structure, scene, random);
}

TYPED_TEST(Spatial3DTest, FollowsEntriesThatMoveComeAndGo) {
    math::Random random(32);
    typename TestFixture::Scene scene = TestFixture::randomScene(random, 400);
    TypeParam structure = TestFixture::make();
    structure.setAll(scene.ids, scene.boxes);
    TestFixture::ready(structure);

    for (std::size_t index = 0; index < scene.boxes.size(); index += 2) {
        scene.boxes[index] = math::Box::fromCenter(scene.boxes[index].getCenter() + TestFixture::randomPoint(random, 6.0F), scene.boxes[index].getSize());
        structure.set(scene.ids[index], scene.boxes[index]);
    }

    if constexpr (TestFixture::kBuilds) {
        EXPECT_FALSE(structure.isBuilt());
        structure.refit();
    }

    TestFixture::expectMatchesBruteForce(structure, scene, random);

    // Removing most entries folds an octree back together and keeps every other structure exact.
    for (std::size_t index = scene.ids.size(); index-- > 40;) {
        EXPECT_TRUE(structure.remove(scene.ids[index]));
        scene.ids.pop_back();
        scene.boxes.pop_back();
    }

    EXPECT_FALSE(structure.remove(1));
    TestFixture::ready(structure);
    EXPECT_EQ(structure.size(), 40U);
    TestFixture::expectMatchesBruteForce(structure, scene, random);
    EXPECT_EQ(structure.getBounds(scene.ids[3]), scene.boxes[3]);
    EXPECT_FALSE(structure.getBounds(2).has_value());

    structure.clear();
    TestFixture::ready(structure);
    typename TestFixture::Ids ids;
    structure.query({{-100.0F, -100.0F, -100.0F}, {100.0F, 100.0F, 100.0F}}, ids);
    EXPECT_TRUE(ids.empty());
    EXPECT_FALSE(structure.contains(scene.ids.front()));
}

TYPED_TEST(Spatial3DTest, RefusesBoundsWithoutMeaningAndKeepsARefusedBatchOut) {
    TypeParam structure = TestFixture::make();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(structure.set(1, {{1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(structure.set(1, {{notANumber, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(structure.set(1, {{0.0F, 0.0F, 0.0F}, {std::numeric_limits<float>::infinity(), 1.0F, 1.0F}}), std::invalid_argument);

    const std::vector<std::uint64_t> ids{1, 2, 3};
    const std::vector<math::Box> boxes{math::Box{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, math::Box{{notANumber, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, math::Box{{2.0F, 2.0F, 2.0F}, {3.0F, 3.0F, 3.0F}}};
    EXPECT_THROW(structure.setAll(ids, boxes), std::invalid_argument);
    EXPECT_THROW(structure.setAll(ids, std::vector<math::Box>(2)), std::invalid_argument);
    EXPECT_EQ(structure.size(), 0U);

    TestFixture::ready(structure);
    std::vector<std::uint64_t> found;
    EXPECT_THROW(structure.query({{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 0.0F}}, found), std::invalid_argument);
    EXPECT_THROW(structure.querySphere({{0.0F, 0.0F, 0.0F}, -1.0F}, found), std::invalid_argument);
    std::vector<Neighbor> neighbors;
    EXPECT_THROW(structure.nearest({notANumber, 0.0F, 0.0F}, 1, 1.0F, neighbors), std::invalid_argument);
    EXPECT_THROW(structure.nearest({}, 1, -1.0F, neighbors), std::invalid_argument);
}

TYPED_TEST(Spatial3DTest, RepeatsQueriesWithoutAllocating) {
    math::Random random(33);
    const typename TestFixture::Scene scene = TestFixture::randomScene(random, 300);
    TypeParam structure = TestFixture::make();
    structure.setAll(scene.ids, scene.boxes);
    TestFixture::ready(structure);
    const math::Frustum frustum = TestFixture::randomFrustum(random);
    const math::Ray3D ray = TestFixture::randomRay(random);
    const math::Box area{{-20.0F, -20.0F, -20.0F}, {20.0F, 20.0F, 20.0F}};

    typename TestFixture::Ids ids;
    std::vector<RayHit> hits;
    std::vector<Neighbor> neighbors;
    // clang-format off
    const auto queries = [&] {
        structure.query(area, ids);
        structure.querySphere({{}, 25.0F}, ids);
        structure.queryFrustum(frustum, ids);
        structure.raycast(ray, 0, hits);
        structure.nearest({1.0F, 2.0F, 3.0F}, 8, std::numeric_limits<float>::infinity(), neighbors);
    };
    // clang-format on

    // The first round gives the lists the room that the answers take, and the rounds after it reuse that room.
    queries();
    const test::AllocationTracker tracker;
    queries();
    queries();
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TYPED_TEST(Spatial3DTest, KeepsBoxesAtTheEdgesOfTheFloatRange) {
    // Boxes as large as floats allow leave the area heuristic without finite costs, so the trees still split and answer, and cells as large as the boxes hold them in a spatial hash.
    TypeParam structure = TestFixture::makeForHugeBoxes();
    std::vector<std::uint64_t> ids;
    std::vector<math::Box> boxes;
    for (std::uint64_t id = 0; id < 40; ++id) {
        const float offset = static_cast<float>(id);
        const float side = id % 2 == 0 ? 3e38F : 1.0F + offset;
        const float bottom = id % 2 == 0 ? -3e38F : -1.0F;
        ids.push_back(id);
        boxes.push_back({{-side, -side, bottom}, {side, side, side}});
    }

    structure.setAll(ids, boxes);
    TestFixture::ready(structure);
    std::vector<std::uint64_t> found;
    structure.queryPoint({0.5F, 0.5F, 0.5F}, found);
    EXPECT_EQ(found.size(), 40U);
    structure.queryPoint({1e30F, 0.0F, 0.0F}, found);
    EXPECT_EQ(found.size(), 20U);
    std::vector<Neighbor> neighbors;
    structure.nearest({0.0F, 0.0F, -2e38F}, 3, std::numeric_limits<float>::infinity(), neighbors);
    ASSERT_EQ(neighbors.size(), 3U);
    EXPECT_EQ(neighbors[0].id, 0U);
    EXPECT_EQ(neighbors[2].id, 4U);
    EXPECT_EQ(neighbors[2].distance, 0.0F);
}

TEST(HashGrid3DTest, RefusesEntriesTooLargeOrTooFarForItsCells) {
    HashGrid grid(1.0F);
    EXPECT_THROW(grid.set(1, {{0.0F, 0.0F, 0.0F}, {100.0F, 100.0F, 100.0F}}), std::invalid_argument);
    EXPECT_THROW(grid.set(1, {{3e38F, 0.0F, 0.0F}, {3e38F, 1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(grid.set(1, {{2e6F, 0.0F, 0.0F}, {2e6F, 1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_FALSE(grid.contains(1));
    EXPECT_THROW(HashGrid(0.0F), std::invalid_argument);
    EXPECT_THROW(HashGrid{std::numeric_limits<float>::infinity()}, std::invalid_argument);

    grid.set(1, {{-1e6F, -1e6F, -1e6F}, {-1e6F, -1e6F, -1e6F}});
    grid.set(2, {{0.0F, 0.0F, 0.0F}, {39.0F, 39.0F, 39.0F}});
    std::vector<std::uint64_t> found;
    grid.query({{-2e9F, -2e9F, -2e9F}, {2e9F, 2e9F, 2e9F}}, found);
    EXPECT_EQ(found, (std::vector<std::uint64_t>{1, 2}));
}

TEST(OctreeTest, SplitsByItsSettingsAndFoldsBackTogether) {
    EXPECT_THROW(Octree({{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(Octree({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, {.maxEntries = 0}), std::invalid_argument);
    EXPECT_THROW(Octree({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, {.maxDepth = 17}), std::invalid_argument);
    EXPECT_THROW(Octree({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, {.looseness = 5.0F}), std::invalid_argument);

    Octree tree({{0.0F, 0.0F, 0.0F}, {64.0F, 64.0F, 64.0F}}, {.maxEntries = 2, .maxDepth = 4, .looseness = 0.0F});
    for (std::uint64_t id = 0; id < 16; ++id) {
        const float place = static_cast<float>(id) * 4.0F;
        tree.set(id, {{place, place, place}, {place + 1.0F, place + 1.0F, place + 1.0F}});
    }

    EXPECT_GT(tree.getNodeCount(), 1U);
    for (std::uint64_t id = 0; id < 15; ++id) {
        EXPECT_TRUE(tree.remove(id));
    }

    EXPECT_EQ(tree.getNodeCount(), 1U);
    EXPECT_EQ(tree.getRegion(), (math::Box{{0.0F, 0.0F, 0.0F}, {64.0F, 64.0F, 64.0F}}));
}

TEST(AabbTree3DTest, StaysBalancedAndSkipsMovesInsideItsMargin) {
    AabbTree tree(0.5F);
    EXPECT_THROW(AabbTree(-1.0F), std::invalid_argument);
    for (std::uint64_t id = 0; id < 1024; ++id) {
        const float place = static_cast<float>(id);
        tree.set(id, {{place, 0.0F, 0.0F}, {place + 0.5F, 0.5F, 0.5F}});
    }

    EXPECT_LE(tree.getHeight(), 20);
    EXPECT_EQ(tree.getMargin(), 0.5F);
    tree.set(3, {{3.2F, 0.2F, 0.2F}, {3.7F, 0.7F, 0.7F}});
    EXPECT_EQ(tree.getBounds(3), (math::Box{{3.2F, 0.2F, 0.2F}, {3.7F, 0.7F, 0.7F}}));
}

TEST(BvhTest, RefusesQueriesOnATreeThatChangedSinceItsBuild) {
    Bvh tree;
    tree.set(1, {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}});
    std::vector<std::uint64_t> found;
    EXPECT_FALSE(tree.isBuilt());
    EXPECT_THROW(tree.query({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, found), std::logic_error);
    EXPECT_THROW(tree.refit(), std::logic_error);

    tree.build();
    tree.set(1, {{5.0F, 0.0F, 0.0F}, {6.0F, 1.0F, 1.0F}});
    EXPECT_THROW(tree.query({{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}}, found), std::logic_error);
    tree.refit();
    tree.query({{5.5F, 0.0F, 0.0F}, {5.5F, 0.0F, 0.0F}}, found);
    EXPECT_EQ(found, (std::vector<std::uint64_t>{1}));

    // A thousand boxes in one place have no axis to split along, and the median keeps the tree shallow.
    for (std::uint64_t id = 2; id < 1002; ++id) {
        tree.set(id, {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}});
    }

    tree.build();
    EXPECT_LE(tree.getDepth(), 12);
    EXPECT_GT(tree.getNodeCount(), 1U);
}

} // namespace haylen::spatial3d
