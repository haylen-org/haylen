#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/spatial/AabbTree.hpp"
#include "haylen/2d/spatial/HashGrid.hpp"
#include "haylen/2d/spatial/KdTree.hpp"
#include "haylen/2d/spatial/QuadTree.hpp"
#include "haylen/2d/spatial/ScreenPicker.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Raycast.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

using Ids = std::vector<std::uint64_t>;

namespace {

// Answers every query by testing every entry, which the structures must match exactly.
struct Reference {
    std::vector<std::pair<std::uint64_t, math::Rect>> entries;

    void set(std::uint64_t id, const math::Rect& bounds) {
        std::erase_if(entries, [id](const auto& entry) { return entry.first == id; });
        entries.emplace_back(id, bounds);
    }

    [[nodiscard]] Ids query(const math::Rect& area) const {
        Ids ids;
        for (const auto& [id, bounds] : entries) {
            if (bounds.getLeft() <= area.getRight() && area.getLeft() <= bounds.getRight() && bounds.getTop() <= area.getBottom() && area.getTop() <= bounds.getBottom()) {
                ids.push_back(id);
            }
        }
        std::ranges::sort(ids);
        return ids;
    }

    [[nodiscard]] Ids queryCircle(math::Vec2 center, float radius) const {
        Ids ids;
        for (const auto& [id, bounds] : entries) {
            if (math::Vec2::distanceSquared(bounds.clamp(center), center) <= radius * radius) {
                ids.push_back(id);
            }
        }
        std::ranges::sort(ids);
        return ids;
    }

    [[nodiscard]] std::vector<spatial2d::RayHit> raycast(const math::Ray& ray, std::size_t limit) const {
        std::vector<spatial2d::RayHit> hits;
        for (const auto& [id, bounds] : entries) {
            if (const std::optional<math::RayHit> hit = math::Raycast::rect(ray, bounds)) {
                hits.push_back({.id = id, .point = hit->point, .normal = hit->normal, .distance = hit->distance});
            }
        }
        std::ranges::sort(hits, [](const auto& lhs, const auto& rhs) { return lhs.distance != rhs.distance ? lhs.distance < rhs.distance : lhs.id < rhs.id; });
        if (limit > 0 && hits.size() > limit) {
            hits.resize(limit);
        }
        return hits;
    }

    [[nodiscard]] std::vector<spatial2d::Neighbor> nearest(math::Vec2 point, std::size_t count, float maxDistance) const {
        std::vector<spatial2d::Neighbor> neighbors;
        for (const auto& [id, bounds] : entries) {
            const float distance = std::sqrt(math::Vec2::distanceSquared(bounds.clamp(point), point));
            if (distance <= maxDistance) {
                neighbors.push_back({.id = id, .distance = distance});
            }
        }
        std::ranges::sort(neighbors, [](const auto& lhs, const auto& rhs) { return lhs < rhs; });
        if (neighbors.size() > count) {
            neighbors.resize(count);
        }
        return neighbors;
    }
};

template <typename Structure> Structure makeStructure();
template <> spatial2d::HashGrid makeStructure() {
    return spatial2d::HashGrid(24.0F);
}
template <> spatial2d::QuadTree makeStructure() {
    return spatial2d::QuadTree({-400.0F, -400.0F, 800.0F, 800.0F}, {.maxEntries = 4, .maxDepth = 6});
}
template <> spatial2d::AabbTree makeStructure() {
    return spatial2d::AabbTree(3.0F);
}

math::Rect randomBounds(math::Random& random) {
    // Some entries fall outside the quadtree area and some have no size, which every structure must handle.
    return {random.range(-500.0F, 500.0F), random.range(-500.0F, 500.0F), random.chance(0.2F) ? 0.0F : random.range(0.0F, 40.0F), random.chance(0.2F) ? 0.0F : random.range(0.0F, 40.0F)};
}

template <typename Structure> void expectSameAnswers(const Structure& structure, const Reference& reference, math::Random& random) {
    Ids ids;
    std::vector<spatial2d::RayHit> hits;
    std::vector<spatial2d::Neighbor> neighbors;
    for (int probe = 0; probe < 40; ++probe) {
        const math::Rect area = randomBounds(random).expanded(random.range(0.0F, 60.0F));
        structure.query(area, ids);
        EXPECT_EQ(ids, reference.query(area));

        const math::Vec2 point{random.range(-550.0F, 550.0F), random.range(-550.0F, 550.0F)};
        const float radius = random.range(0.0F, 120.0F);
        structure.queryCircle(point, radius, ids);
        EXPECT_EQ(ids, reference.queryCircle(point, radius));
        structure.queryPoint(point, ids);
        EXPECT_EQ(ids, reference.query({point.x, point.y, 0.0F, 0.0F}));

        const math::Ray ray = math::Ray::between(point, {random.range(-550.0F, 550.0F), random.range(-550.0F, 550.0F)});
        const std::size_t limit = static_cast<std::size_t>(random.range(0, 3));
        structure.raycast(ray, limit, hits);
        const std::vector<spatial2d::RayHit> expectedHits = reference.raycast(ray, limit);
        ASSERT_EQ(hits.size(), expectedHits.size());
        for (std::size_t index = 0; index < hits.size(); ++index) {
            EXPECT_EQ(hits[index].id, expectedHits[index].id);
            EXPECT_EQ(hits[index].distance, expectedHits[index].distance);
            EXPECT_EQ(hits[index].normal, expectedHits[index].normal);
        }

        // Small counts stop early, and a count of every entry ranks the whole structure.
        const std::size_t count = probe % 10 == 0 ? reference.entries.size() : static_cast<std::size_t>(random.range(1, 6));
        const float maxDistance = random.chance(0.5F) ? std::numeric_limits<float>::infinity() : random.range(0.0F, 200.0F);
        structure.nearest(point, count, maxDistance, neighbors);
        const std::vector<spatial2d::Neighbor> expectedNeighbors = reference.nearest(point, count, maxDistance);
        ASSERT_EQ(neighbors.size(), expectedNeighbors.size());
        for (std::size_t index = 0; index < neighbors.size(); ++index) {
            EXPECT_EQ(neighbors[index].id, expectedNeighbors[index].id);
            EXPECT_EQ(neighbors[index].distance, expectedNeighbors[index].distance);
        }
    }

    const math::Rect everything{-1000.0F, -1000.0F, 2000.0F, 2000.0F};
    structure.query(everything, ids);
    EXPECT_EQ(ids, reference.query(everything));
}

} // namespace

template <typename Structure> class SpatialStructureTest : public ::testing::Test {};
using SpatialStructures = ::testing::Types<spatial2d::HashGrid, spatial2d::QuadTree, spatial2d::AabbTree>;
TYPED_TEST_SUITE(SpatialStructureTest, SpatialStructures);

TYPED_TEST(SpatialStructureTest, AnswersLikeTestingEveryEntry) {
    TypeParam structure = makeStructure<TypeParam>();
    Reference reference;
    math::Random random(42);
    for (std::uint64_t id = 1; id <= 300; ++id) {
        const math::Rect bounds = randomBounds(random);
        structure.set(id, bounds);
        reference.set(id, bounds);
    }
    EXPECT_EQ(structure.size(), 300U);
    expectSameAnswers(structure, reference, random);

    // Small moves stay in place, large moves travel, and removals leave the rest intact.
    for (std::uint64_t id = 1; id <= 300; id += 3) {
        const math::Rect bounds = id % 2 == 0 ? reference.entries[id - 1].second.translated({random.range(-2.0F, 2.0F), random.range(-2.0F, 2.0F)}) : randomBounds(random);
        structure.set(id, bounds);
        reference.set(id, bounds);
    }
    for (std::uint64_t id = 2; id <= 300; id += 4) {
        EXPECT_TRUE(structure.remove(id));
        std::erase_if(reference.entries, [id](const auto& entry) { return entry.first == id; });
    }
    EXPECT_FALSE(structure.remove(2));
    EXPECT_FALSE(structure.contains(2));
    EXPECT_TRUE(structure.contains(3));
    EXPECT_EQ(structure.size(), reference.entries.size());
    expectSameAnswers(structure, reference, random);

    structure.clear();
    Ids ids;
    structure.query({-1000.0F, -1000.0F, 2000.0F, 2000.0F}, ids);
    EXPECT_TRUE(ids.empty());
    EXPECT_EQ(structure.size(), 0U);
}

TYPED_TEST(SpatialStructureTest, CountsTouchingBoundsAndRejectsInvalidInput) {
    TypeParam structure = makeStructure<TypeParam>();
    structure.set(7, {10.0F, 10.0F, 0.0F, 0.0F});
    structure.set(8, {30.0F, 0.0F, 10.0F, 10.0F});
    EXPECT_EQ(structure.getBounds(8), (math::Rect{30.0F, 0.0F, 10.0F, 10.0F}));
    EXPECT_FALSE(structure.getBounds(9).has_value());

    Ids ids;
    structure.queryPoint({10.0F, 10.0F}, ids);
    EXPECT_EQ(ids, (Ids{7}));
    structure.query({0.0F, 0.0F, 10.0F, 10.0F}, ids);
    EXPECT_EQ(ids, (Ids{7}));
    structure.queryCircle({25.0F, 5.0F}, 5.0F, ids);
    EXPECT_EQ(ids, (Ids{8}));
    structure.queryCircle({25.0F, 5.0F}, 4.9F, ids);
    EXPECT_TRUE(ids.empty());

    std::vector<spatial2d::RayHit> hits;
    structure.raycast(math::Ray::between({0.0F, 5.0F}, {100.0F, 5.0F}), 0, hits);
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits[0].id, 8U);
    EXPECT_EQ(hits[0].normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_FLOAT_EQ(hits[0].distance, 30.0F);

    EXPECT_THROW(structure.set(1, {0.0F, 0.0F, -1.0F, 1.0F}), std::invalid_argument);
    EXPECT_THROW(structure.set(1, {std::nanf(""), 0.0F, 1.0F, 1.0F}), std::invalid_argument);
    EXPECT_THROW(structure.queryCircle({0.0F, 0.0F}, -1.0F, ids), std::invalid_argument);
    std::vector<spatial2d::Neighbor> neighbors;
    EXPECT_THROW(structure.nearest({0.0F, 0.0F}, 1, -1.0F, neighbors), std::invalid_argument);
    EXPECT_THROW(structure.nearest({std::nanf(""), 0.0F}, 1, 10.0F, neighbors), std::invalid_argument);
    EXPECT_THROW(structure.nearest({0.0F, std::numeric_limits<float>::infinity()}, 1, 10.0F, neighbors), std::invalid_argument);
    EXPECT_EQ(structure.size(), 2U);
}

TEST(HashGridTest, SparseEntriesKeepQueriesProportionalToTheEntries) {
    spatial2d::HashGrid hash(1.0F);
    hash.set(1, {0.0F, 0.0F, 1.0F, 1.0F});
    hash.set(2, {1e8F, 1e8F, 1.0F, 1.0F});

    // The areas span far more cells than any grid could visit, and the nearest point lies hundreds of millions of cells from both entries.
    Ids ids;
    hash.query({-1e9F, -1e9F, 2e9F, 2e9F}, ids);
    EXPECT_EQ(ids, (Ids{1, 2}));
    hash.query({-3e38F, -3e38F, 3e38F, 3e38F}, ids);
    EXPECT_EQ(ids, (Ids{1}));
    hash.queryCircle({5e7F, 5e7F}, 1e8F, ids);
    EXPECT_EQ(ids, (Ids{1, 2}));
    hash.queryPoint({1e30F, 0.0F}, ids);
    EXPECT_TRUE(ids.empty());

    std::vector<spatial2d::Neighbor> neighbors;
    hash.nearest({5e8F, 5e8F}, 1, std::numeric_limits<float>::infinity(), neighbors);
    ASSERT_EQ(neighbors.size(), 1U);
    EXPECT_EQ(neighbors[0].id, 2U);
    hash.nearest({3e38F, 0.0F}, 2, 1000.0F, neighbors);
    EXPECT_TRUE(neighbors.empty());
    hash.nearest({-3e38F, 0.0F}, 2, std::numeric_limits<float>::infinity(), neighbors);
    EXPECT_EQ(neighbors.size(), 2U);

    // Rays across the empty cells between the entries test the entries instead of walking the cells.
    std::vector<spatial2d::RayHit> hits;
    hash.raycast(math::Ray::between({-10.0F, 0.5F}, {2e8F, 0.5F}), 0, hits);
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits[0].id, 1U);
    EXPECT_FLOAT_EQ(hits[0].distance, 10.0F);
    hash.raycast(math::Ray{{-10.0F, -10.0F}, math::Vec2{1.0F, 1.0F}.getNormalized()}, 0, hits);
    ASSERT_EQ(hits.size(), 2U);
    EXPECT_EQ(hits[0].id, 1U);
    EXPECT_EQ(hits[1].id, 2U);
}

TEST(HashGridTest, RejectsEntriesTooLargeOrTooFarForTheCells) {
    spatial2d::HashGrid hash(1.0F);
    EXPECT_THROW(hash.set(1, {0.0F, 0.0F, 1e6F, 1e6F}), std::invalid_argument);
    EXPECT_THROW(hash.set(1, {1e12F, 0.0F, 1.0F, 1.0F}), std::invalid_argument);
    EXPECT_THROW(hash.set(1, {3e38F, 0.0F, 3e38F, 1.0F}), std::invalid_argument);
    EXPECT_FALSE(hash.contains(1));

    hash.set(1, {0.0F, 0.0F, 255.0F, 255.0F});
    EXPECT_EQ(hash.size(), 1U);
}

TEST(HashGridTest, CastsEndlessRaysAndRejectsInvalidCellSizes) {
    EXPECT_THROW(spatial2d::HashGrid(0.0F), std::invalid_argument);
    EXPECT_THROW(spatial2d::HashGrid{std::numeric_limits<float>::infinity()}, std::invalid_argument);

    // Rays only travel across the cells that ever held entries, so a ray without an end still stops.
    spatial2d::HashGrid hash(4.0F);
    hash.set(1, {0.0F, 0.0F, 2.0F, 2.0F});
    std::vector<spatial2d::RayHit> hits;
    hash.raycast(math::Ray{{-10.0F, 1.0F}, {1.0F, 0.0F}}, 0, hits);
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_FLOAT_EQ(hits[0].distance, 10.0F);
    hash.raycast(math::Ray{{-10.0F, 100.0F}, {1.0F, 0.0F}}, 0, hits);
    EXPECT_TRUE(hits.empty());
    EXPECT_EQ(hash.getCellSize(), 4.0F);
}

TEST(QuadTreeTest, SplitsFullQuadrantsAndMergesEmptyOnes) {
    spatial2d::QuadTree tree({0.0F, 0.0F, 100.0F, 100.0F}, {.maxEntries = 2, .maxDepth = 3});
    EXPECT_EQ(tree.getArea(), (math::Rect{0.0F, 0.0F, 100.0F, 100.0F}));
    EXPECT_EQ(tree.getNodeCount(), 1U);
    tree.set(1, {10.0F, 10.0F, 1.0F, 1.0F});
    tree.set(2, {12.0F, 12.0F, 1.0F, 1.0F});
    tree.set(3, {80.0F, 80.0F, 1.0F, 1.0F});
    EXPECT_EQ(tree.getNodeCount(), 5U);

    // Entries crowded in one corner keep splitting down to the depth limit.
    tree.set(4, {11.0F, 11.0F, 1.0F, 1.0F});
    EXPECT_GT(tree.getNodeCount(), 5U);
    EXPECT_TRUE(tree.remove(4));
    EXPECT_TRUE(tree.remove(1));
    EXPECT_EQ(tree.getNodeCount(), 1U);

    // An entry across the middle stays at the root, and one outside the area too.
    tree.set(5, {45.0F, 45.0F, 10.0F, 10.0F});
    tree.set(6, {500.0F, 500.0F, 1.0F, 1.0F});
    Ids ids;
    tree.queryPoint({500.0F, 500.0F}, ids);
    EXPECT_EQ(ids, (Ids{6}));

    EXPECT_THROW(spatial2d::QuadTree({0.0F, 0.0F, 0.0F, 10.0F}), std::invalid_argument);
    EXPECT_THROW(spatial2d::QuadTree({0.0F, 0.0F, 10.0F, 10.0F}, {.maxEntries = 0}), std::invalid_argument);
}

TEST(QuadTreeTest, StopsSplittingCoincidentEntriesAtTheDepthLimit) {
    spatial2d::QuadTree tree({0.0F, 0.0F, 100.0F, 100.0F}, {.maxEntries = 1, .maxDepth = spatial2d::QuadTree::kMaxDepth});
    for (std::uint64_t id = 1; id <= 3; ++id) {
        tree.set(id, {10.0F, 10.0F, 0.0F, 0.0F});
    }
    EXPECT_EQ(tree.getNodeCount(), 1U + 4U * 16U);
    Ids ids;
    tree.queryPoint({10.0F, 10.0F}, ids);
    EXPECT_EQ(ids, (Ids{1, 2, 3}));

    // Deeper limits are refused, since entries at one spot would split down to them.
    EXPECT_THROW(spatial2d::QuadTree({0.0F, 0.0F, 10.0F, 10.0F}, {.maxDepth = 17}), std::invalid_argument);
    EXPECT_THROW(spatial2d::QuadTree({0.0F, 0.0F, 10.0F, 10.0F}, {.maxDepth = 1000000}), std::invalid_argument);
}

TEST(AabbTreeTest, StaysBalancedAndSkipsSmallMoves) {
    spatial2d::AabbTree tree(5.0F);
    EXPECT_EQ(tree.getMargin(), 5.0F);
    EXPECT_EQ(tree.getHeight(), 0);
    for (std::uint64_t id = 0; id < 1024; ++id) {
        // Entries added in sorted order are the worst case for an unbalanced tree.
        tree.set(id, {static_cast<float>(id) * 10.0F, 0.0F, 5.0F, 5.0F});
    }
    EXPECT_LE(tree.getHeight(), 20);

    tree.set(3, {32.0F, 2.0F, 5.0F, 5.0F});
    EXPECT_EQ(tree.getBounds(3), (math::Rect{32.0F, 2.0F, 5.0F, 5.0F}));
    Ids ids;
    tree.queryPoint({36.0F, 6.0F}, ids);
    EXPECT_EQ(ids, (Ids{3}));

    for (std::uint64_t id = 0; id < 1024; id += 2) {
        tree.remove(id);
    }
    EXPECT_LE(tree.getHeight(), 20);
    EXPECT_THROW(spatial2d::AabbTree(-1.0F), std::invalid_argument);
}

TEST(KdTreeTest, FindsNearestPointsLikeTestingEveryEntry) {
    spatial2d::KdTree tree;
    math::Random random(7);
    std::vector<std::pair<std::uint64_t, math::Vec2>> points;
    for (std::uint64_t id = 1; id <= 500; ++id) {
        points.emplace_back(id, math::Vec2{random.range(-100.0F, 100.0F), random.range(-100.0F, 100.0F)});
        tree.set(id, points.back().second);
    }
    std::vector<spatial2d::Neighbor> neighbors;
    EXPECT_FALSE(tree.isBuilt());
    EXPECT_THROW(tree.nearest({0.0F, 0.0F}, 1, 10.0F, neighbors), std::logic_error);
    tree.build();
    EXPECT_TRUE(tree.isBuilt());

    for (int probe = 0; probe < 50; ++probe) {
        const math::Vec2 point{random.range(-120.0F, 120.0F), random.range(-120.0F, 120.0F)};
        tree.nearest(point, 5, std::numeric_limits<float>::infinity(), neighbors);
        std::vector<spatial2d::Neighbor> expected;
        for (const auto& [id, position] : points) {
            expected.push_back({.id = id, .distance = math::Vec2::distance(point, position)});
        }
        std::ranges::sort(expected, [](const auto& lhs, const auto& rhs) { return lhs < rhs; });
        expected.resize(5);
        ASSERT_EQ(neighbors.size(), 5U);
        for (std::size_t index = 0; index < 5; ++index) {
            EXPECT_EQ(neighbors[index].id, expected[index].id);
        }

        Ids ids;
        tree.queryCircle(point, 20.0F, ids);
        Ids inside;
        for (const auto& [id, position] : points) {
            if (math::Vec2::distance(point, position) <= 20.0F) {
                inside.push_back(id);
            }
        }
        std::ranges::sort(inside);
        EXPECT_EQ(ids, inside);
    }
}

TEST(KdTreeTest, TreatsEntriesWithRadiusAsCircles) {
    spatial2d::KdTree tree;
    tree.set(1, {50.0F, 0.0F}, 10.0F);
    tree.set(2, {100.0F, 0.0F}, 5.0F);
    tree.set(3, {0.0F, 80.0F});
    tree.build();
    EXPECT_EQ(tree.getPoint(1), (math::Vec2{50.0F, 0.0F}));
    EXPECT_EQ(tree.size(), 3U);

    std::vector<spatial2d::RayHit> hits;
    tree.raycast(math::Ray::between({0.0F, 0.0F}, {200.0F, 0.0F}), 0, hits);
    ASSERT_EQ(hits.size(), 2U);
    EXPECT_EQ(hits[0].id, 1U);
    EXPECT_FLOAT_EQ(hits[0].distance, 40.0F);
    EXPECT_FLOAT_EQ(hits[1].distance, 95.0F);
    tree.raycast(math::Ray{{0.0F, 0.0F}, {1.0F, 0.0F}}, 1, hits);
    EXPECT_EQ(hits.size(), 1U);

    Ids ids;
    tree.queryPoint({55.0F, 5.0F}, ids);
    EXPECT_EQ(ids, (Ids{1}));
    tree.query({0.0F, 75.0F, 5.0F, 10.0F}, ids);
    EXPECT_EQ(ids, (Ids{3}));

    std::vector<spatial2d::Neighbor> neighbors;
    tree.nearest({70.0F, 0.0F}, 1, 100.0F, neighbors);
    EXPECT_EQ(neighbors.front().id, 1U);
    EXPECT_FLOAT_EQ(neighbors.front().distance, 10.0F);
    EXPECT_THROW(tree.nearest({std::nanf(""), 0.0F}, 1, 100.0F, neighbors), std::invalid_argument);

    EXPECT_TRUE(tree.remove(1));
    EXPECT_FALSE(tree.remove(1));
    EXPECT_THROW(tree.queryPoint({0.0F, 0.0F}, ids), std::logic_error);
    EXPECT_THROW(tree.set(4, {0.0F, 0.0F}, -1.0F), std::invalid_argument);
    tree.clear();
    EXPECT_TRUE(tree.isBuilt());
    EXPECT_FALSE(tree.contains(2));
}

TEST(ScreenPickerTest, MapsScreenPointsThroughTheCameraViewport) {
    graphics2d::Camera camera;
    camera.position = {1000.0F, 500.0F};
    camera.viewport = math::Rect{400.0F, 0.0F, 400.0F, 600.0F};
    camera.setZoom({2.0F, 2.0F});
    const spatial2d::ScreenPicker picker(camera, {0.0F, 0.0F, 800.0F, 600.0F});

    EXPECT_NEAR(picker.toWorld({600.0F, 300.0F}).x, 1000.0F, 1e-3F);
    EXPECT_NEAR(picker.toWorld({700.0F, 300.0F}).x, 1050.0F, 1e-3F);
    EXPECT_NEAR(picker.toWorld({700.0F, 300.0F}).y, 500.0F, 1e-3F);

    const math::Ray aim = picker.toRay({700.0F, 300.0F});
    EXPECT_NEAR(aim.origin.x, 1000.0F, 1e-3F);
    EXPECT_NEAR(aim.direction.x, 1.0F, 1e-5F);
    EXPECT_NEAR(aim.length, 50.0F, 1e-3F);

    spatial2d::HashGrid grid(32.0F);
    grid.set(7, {1045.0F, 495.0F, 10.0F, 10.0F});
    std::vector<std::uint64_t> ids;
    picker.pick(grid, {700.0F, 300.0F}, ids);
    EXPECT_EQ(ids, std::vector<std::uint64_t>{7});
    picker.pick(grid, {600.0F, 300.0F}, ids);
    EXPECT_TRUE(ids.empty());
}

TEST(Spatial2DLuaTest, PicksAndAimsFromScreenPoints) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        camera = require('haylen.graphics2d').newCamera()
        camera.position = {1000, 500}
        local screen = require('haylen.viewport').visibleRect()
        cx, cy = screen.x + screen.width / 2, screen.y + screen.height / 2
        wx, wy = camera:screenToWorld(cx + 50, cy)
        crate = {name = 'crate'}
        hash = spatial2d.newHash(32)
        hash:set(crate, {wx - 5, wy - 5, 10, 10})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local x1, y1, x2, y2 = spatial2d.screenRay(camera, cx + 50, cy) local ox, oy = camera:screenToWorld(cx, cy) return tostring(x1 == ox and y1 == oy and math.abs(x2 - wx) < 1e-3 and math.abs(y2 - wy) < 1e-3)"), "true");
    EXPECT_EQ(fixture.lua("return #hash:pick(camera, cx + 50, cy) .. ' ' .. hash:pick(camera, cx + 50, cy)[1].name .. ' ' .. #hash:pick(camera, cx - 300, cy)"), "1 crate 0");
    EXPECT_EQ(fixture.lua("local tree = spatial2d.newAabbTree() tree:set(crate, {wx - 5, wy - 5, 10, 10}) return #tree:pick(camera, cx + 50, cy)"), "1");
}

TEST(Spatial2DLuaTest, StoresLuaValuesInEveryStructure) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        function names(values)
            local list = {}
            for _, value in ipairs(values) do list[#list + 1] = value.name end
            return table.concat(list, ',')
        end
        function hitNames(hits)
            local list = {}
            for _, hit in ipairs(hits) do list[#list + 1] = hit.value.name end
            return table.concat(list, ',')
        end
        function fill(structure)
            goblin = {name = 'goblin'}
            tree = {name = 'tree'}
            rock = {name = 'rock'}
            structure:set(goblin, {0, 0, 10, 10})
            structure:set(tree, {x = 40, y = 0, width = 20, height = 20})
            structure:set(rock, {200, 200, 8, 8})
            return structure
        end
        function check(structure)
            fill(structure)
            local results = {
                structure.size,
                names(structure:query({0, 0, 50, 50})),
                names(structure:queryCircle(204, 204, 1)),
                names(structure:queryPoint(5, 5)),
                hitNames(structure:raycast(-10, 5, 300, 5)),
                hitNames(structure:raycast(-10, 5, 300, 5, 1)),
                structure:raycast(-10, 5, 300, 5)[2].x,
                structure:nearest(0, 0, 500).name,
                structure:nearest(0, 0, 500, function(value) return value ~= goblin end).name,
                tostring(structure:nearest(300, 300, 5)),
                names(structure:kNearest(190, 190, 2)),
                names(structure:kNearest(190, 190, 5, 100)),
                tostring(structure:remove(tree)) .. tostring(structure:remove(tree)) .. tostring(structure:has(tree)),
            }
            structure:clear()
            results[#results + 1] = structure.size .. tostring(structure:has(rock))
            return table.concat(results, ' ')
        end
    )");
    // clang-format on

    const std::string expected = "3 goblin,tree rock goblin goblin,tree goblin 40.0 goblin tree nil rock,tree rock truefalsefalse 0false";
    EXPECT_EQ(fixture.lua("return check(spatial2d.newHash(32))"), expected);
    EXPECT_EQ(fixture.lua("return check(spatial2d.newQuadTree({-500, -500, 1000, 1000}, {maxEntries = 1, maxDepth = 4}))"), expected);
    EXPECT_EQ(fixture.lua("return check(spatial2d.newAabbTree(2))"), expected);
    EXPECT_EQ(fixture.lua("local hash = fill(spatial2d.newHash(16)) return hash.cellSize .. ' ' .. hash:bounds(tree).x .. ' ' .. tostring(hash:bounds({}))"), "16.0 40.0 nil");
    EXPECT_EQ(fixture.lua("local quad = fill(spatial2d.newQuadTree({0, 0, 400, 400}, {maxEntries = 1})) return quad.area.width .. ' ' .. quad.nodeCount"), "400.0 13");
    EXPECT_EQ(fixture.lua("local aabb = fill(spatial2d.newAabbTree()) return aabb.margin .. ' ' .. aabb.height"), "4.0 2");

    // clang-format off
    fixture.runLua(R"(
        points = spatial2d.newKdTree()
        points:set(goblin, 0, 0)
        points:set(tree, 50, 0, 10)
        points:set(rock, 200, 200)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return tostring(points.built)"), "false");
    EXPECT_NE(fixture.lua("points:kNearest(0, 0, 1)").find("needs build"), std::string::npos);
    EXPECT_EQ(fixture.lua("points:build() return names(points:kNearest(45, 0, 2)) .. ' ' .. points:position(tree).x .. ' ' .. names(points:queryPoint(55, 0)) .. ' ' .. hitNames(points:raycast(-5, 0, 100, 0))"), "tree,goblin 50.0 tree goblin,tree");

    EXPECT_NE(fixture.lua("spatial2d.newHash(32):set(nil, {0, 0, 1, 1})").find("a value to store is required"), std::string::npos);
    EXPECT_EQ(fixture.lua("local hash = spatial2d.newHash(8) pcall(hash.set, hash, rock, {0, 0, -1, 1}) return hash:has(rock)"), "false");
    EXPECT_NE(fixture.lua("fill(spatial2d.newAabbTree()):nearest(0, 0, 10, function() error('filter failed') end)").find("filter failed"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newHash(-2)").find("positive cell size"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newQuadTree({0, 0, 10, 10}, {depth = 3})").find("Unknown option 'depth'"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newQuadTree({0, 0, 10, 10}, {maxDepth = 40})").find("a depth from 0 to 16"), std::string::npos);
    EXPECT_NE(fixture.lua("spatial2d.newHash(1):set(rock, {0, 0, 1000, 1000})").find("at most 65536 cells"), std::string::npos);
    EXPECT_NE(fixture.lua("fill(spatial2d.newHash(8)):nearest(0 / 0, 0, 10)").find("point must be finite"), std::string::npos);
}

TEST(Spatial2DLuaTest, NearestNeverOffersValuesThatAcceptRemoved) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        spatial2d = require('haylen.spatial2d')
        hash = spatial2d.newHash(32)
        near, middle, far = {name = 'near'}, {name = 'middle'}, {name = 'far'}
        hash:set(near, {0, 0, 4, 4})
        hash:set(middle, {50, 0, 4, 4})
        hash:set(far, {100, 0, 4, 4})
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local offered = {} local found = hash:nearest(0, 0, 500, function(value) offered[#offered + 1] = value.name if value == near then hash:remove(middle) end return value == far end) return found.name .. ' ' .. table.concat(offered, ',')"), "far near,far");
    EXPECT_EQ(fixture.lua("local offered = 0 local found = hash:nearest(0, 0, 500, function() offered = offered + 1 hash:clear() return false end) return tostring(found) .. ' ' .. offered .. ' ' .. hash.size"), "nil 1 0");
}

} // namespace haylen
