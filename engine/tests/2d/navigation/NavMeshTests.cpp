#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/2d/navigation/NavMesh.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Segment.hpp"

namespace haylen {

class NavMeshTest : public ::testing::Test {
  protected:
    using Polygon = std::vector<math::Vec2>;

    static Polygon box(float x, float y, float width, float height) {
        return {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
    }

    // Adds regular polygons with random centers, sizes and turns inside an 800 by 600 boundary.
    static void addPolygons(navigation2d::NavMesh& mesh, math::Random& random, int count, int corners) {
        for (int obstacle = 0; obstacle < count; ++obstacle) {
            const math::Vec2 center{random.range(50.0F, 750.0F), random.range(50.0F, 550.0F)};
            const float size = random.range(15.0F, 60.0F);
            const float turn = random.range(0.0F, 3.14F);
            Polygon polygon;
            for (int corner = 0; corner < corners; ++corner) {
                polygon.push_back(center + math::Vec2::fromAngle(turn + static_cast<float>(corner) * 2.0F * std::numbers::pi_v<float> / static_cast<float>(corners), size));
            }
            mesh.addObstacle(polygon);
        }
    }

    // Checks that the triangles are counterclockwise in the y-up frame, that neighbors point back at each other and that their areas add up.
    static float checkTriangles(navigation2d::NavMesh& mesh) {
        const std::vector<navigation2d::NavMesh::Triangle>& triangles = mesh.getTriangles();
        const std::vector<math::Vec2>& vertices = mesh.getVertices();
        float area = 0.0F;
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            const navigation2d::NavMesh::Triangle& triangle = triangles[index];
            const math::Vec2 a = vertices[triangle.vertices[0]];
            const math::Vec2 b = vertices[triangle.vertices[1]];
            const math::Vec2 c = vertices[triangle.vertices[2]];
            const float twice = math::Vec2::cross(b - a, c - a);
            EXPECT_GT(twice, 0.0F);
            area += twice * 0.5F;
            for (const std::int32_t neighbor : triangle.neighbors) {
                if (neighbor >= 0) {
                    const std::array<std::int32_t, 3>& back = triangles[static_cast<std::size_t>(neighbor)].neighbors;
                    EXPECT_TRUE(back[0] == static_cast<std::int32_t>(index) || back[1] == static_cast<std::int32_t>(index) || back[2] == static_cast<std::int32_t>(index));
                }
            }
        }
        return area;
    }

    // Returns the walls of the mesh, each with the walkable side on its left.
    static std::vector<math::Segment> collectWalls(navigation2d::NavMesh& mesh) {
        std::vector<math::Segment> walls;
        for (const navigation2d::NavMesh::Triangle& triangle : mesh.getTriangles()) {
            for (std::size_t side = 0; side < 3; ++side) {
                if (triangle.neighbors[side] < 0) {
                    walls.push_back({mesh.getVertices()[triangle.vertices[side]], mesh.getVertices()[triangle.vertices[(side + 1) % 3]]});
                }
            }
        }
        return walls;
    }

    // Returns a point on the walkable side of a random wall, closer to it than the radius.
    static math::Vec2 pointNearWall(std::span<const math::Segment> walls, math::Random& random, float radius) {
        const math::Segment& wall = walls[static_cast<std::size_t>(random.range(0, static_cast<int>(walls.size()) - 1))];
        const math::Vec2 along = math::Vec2::lerp(wall.start, wall.end, random.range(0.0F, 1.0F));
        return along + (wall.end - wall.start).getNormalized().getPerpendicular() * random.range(0.0F, radius);
    }

    static std::vector<math::Vec2> copyPath(std::span<const math::Vec2> path) {
        return {path.begin(), path.end()};
    }

    // Samples every leg of a path and checks that it never leaves the mesh.
    static void expectInside(navigation2d::NavMesh& mesh, std::span<const math::Vec2> path) {
        for (std::size_t index = 1; index < path.size(); ++index) {
            for (int sample = 0; sample <= 20; ++sample) {
                const math::Vec2 point = math::Vec2::lerp(path[index - 1], path[index], static_cast<float>(sample) / 20.0F);
                EXPECT_TRUE(mesh.contains(point)) << point.x << "," << point.y;
            }
        }
    }

    // Checks that no leg of a path comes closer than the radius to any of the corners.
    static void expectClear(std::span<const math::Vec2> path, std::span<const math::Vec2> corners, float radius) {
        for (std::size_t index = 1; index < path.size(); ++index) {
            for (const math::Vec2 corner : corners) {
                EXPECT_GE(math::Geometry::distanceToSegment({path[index - 1], path[index]}, corner), radius - 1e-3F) << corner.x << "," << corner.y;
            }
        }
    }
};

TEST_F(NavMeshTest, CoversTheWalkableAreaAroundObstacles) {
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 400.0F, 300.0F));
    EXPECT_TRUE(mesh.isDirty());
    EXPECT_NEAR(checkTriangles(mesh), 400.0F * 300.0F, 1.0F);
    EXPECT_FALSE(mesh.isDirty());

    const std::uint32_t pillar = mesh.addObstacle(box(150.0F, 100.0F, 100.0F, 100.0F));
    mesh.addObstacle(box(20.0F, 20.0F, 40.0F, 40.0F));
    EXPECT_EQ(mesh.getObstacleCount(), 2U);
    EXPECT_TRUE(mesh.isDirty());
    EXPECT_NEAR(checkTriangles(mesh), 400.0F * 300.0F - 100.0F * 100.0F - 40.0F * 40.0F, 1.0F);
    EXPECT_FALSE(mesh.contains({200.0F, 150.0F}));
    EXPECT_TRUE(mesh.contains({100.0F, 150.0F}));
    EXPECT_FALSE(mesh.contains({500.0F, 150.0F}));
    EXPECT_TRUE(mesh.findTriangle({300.0F, 250.0F}).has_value());

    // Overlapping obstacles and obstacles that stick out of the boundary merge into one blocked area.
    mesh.setObstacle(pillar, box(150.0F, 100.0F, 100.0F, 100.0F));
    mesh.addObstacle(box(200.0F, 150.0F, 100.0F, 100.0F));
    mesh.addObstacle(box(350.0F, -50.0F, 100.0F, 100.0F));
    const float expected = 400.0F * 300.0F - 1600.0F - (10000.0F + 10000.0F - 2500.0F) - 50.0F * 50.0F;
    EXPECT_NEAR(checkTriangles(mesh), expected, 1.0F);
    EXPECT_FALSE(mesh.contains({275.0F, 175.0F}));
    EXPECT_FALSE(mesh.contains({375.0F, 25.0F}));

    EXPECT_TRUE(mesh.removeObstacle(pillar));
    EXPECT_FALSE(mesh.removeObstacle(pillar));
    EXPECT_TRUE(mesh.contains({160.0F, 110.0F}));
    mesh.clearObstacles();
    EXPECT_NEAR(checkTriangles(mesh), 400.0F * 300.0F, 1.0F);
}

TEST_F(NavMeshTest, PullsPathsTightAroundCornersWithTheAgentRadius) {
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 400.0F, 300.0F));
    const std::span<const math::Vec2> open = mesh.findPath({20.0F, 150.0F}, {380.0F, 150.0F});
    ASSERT_EQ(open.size(), 2U);
    EXPECT_FLOAT_EQ(mesh.getPathLength(), 360.0F);

    // A wall across the middle leaves a gap at the bottom, so the path turns at the wall end.
    mesh.addObstacle(box(190.0F, 0.0F, 20.0F, 240.0F));
    const std::vector<math::Vec2> tight = copyPath(mesh.findPath({20.0F, 150.0F}, {380.0F, 150.0F}));
    ASSERT_GE(tight.size(), 3U);
    EXPECT_EQ(tight.front(), (math::Vec2{20.0F, 150.0F}));
    EXPECT_EQ(tight.back(), (math::Vec2{380.0F, 150.0F}));
    expectInside(mesh, tight);
    EXPECT_NEAR(tight[1].y, 240.0F, 0.5F);
    const float tightLength = mesh.getPathLength();
    EXPECT_GT(tightLength, 360.0F);

    // With a radius the corners move off the wall ends, and the path grows longer.
    const std::vector<math::Vec2> padded = copyPath(mesh.findPath({20.0F, 150.0F}, {380.0F, 150.0F}, 10.0F));
    ASSERT_GE(padded.size(), 3U);
    expectInside(mesh, padded);
    for (std::size_t index = 1; index + 1 < padded.size(); ++index) {
        EXPECT_GE(std::min(math::Vec2::distance(padded[index], {190.0F, 240.0F}), math::Vec2::distance(padded[index], {210.0F, 240.0F})), 9.5F);
        EXPECT_GT(padded[index].y, 240.0F);
    }
    EXPECT_GT(mesh.getPathLength(), tightLength);

    // A gap narrower than the agent closes the way.
    EXPECT_TRUE(mesh.findPath({20.0F, 150.0F}, {380.0F, 150.0F}, 40.0F).empty());
    EXPECT_TRUE(std::isinf(mesh.getPathLength()));
    EXPECT_TRUE(mesh.findPath({-20.0F, 150.0F}, {380.0F, 150.0F}).empty());
    EXPECT_THROW((void)mesh.findPath({20.0F, 150.0F}, {380.0F, 150.0F}, -1.0F), std::invalid_argument);

    EXPECT_EQ(mesh.getClosestPoint({100.0F, 100.0F}), (math::Vec2{100.0F, 100.0F}));
    const std::optional<math::Vec2> snapped = mesh.getClosestPoint({200.0F, 100.0F});
    ASSERT_TRUE(snapped.has_value());
    EXPECT_NEAR(std::fabs(snapped->x - 200.0F), 10.0F, 0.01F);
}

TEST_F(NavMeshTest, KeepsEveryLegTheRadiusAwayFromTheCornersItTurnsAround) {
    // A right-angle bend around one corner.
    navigation2d::NavMesh bend;
    bend.setBoundary(box(0.0F, 0.0F, 300.0F, 300.0F));
    bend.addObstacle(box(60.0F, -20.0F, 260.0F, 260.0F));
    const std::vector<math::Vec2> around = copyPath(bend.findPath({30.0F, 20.0F}, {280.0F, 270.0F}, 10.0F));
    ASSERT_GE(around.size(), 3U);
    expectInside(bend, around);
    expectClear(around, std::vector<math::Vec2>{{60.0F, 240.0F}}, 10.0F);

    // A turn back around the end of a thin wall passes both of its corners.
    navigation2d::NavMesh hairpin;
    hairpin.setBoundary(box(0.0F, 0.0F, 300.0F, 300.0F));
    hairpin.addObstacle(box(140.0F, -20.0F, 20.0F, 220.0F));
    const std::vector<math::Vec2> back = copyPath(hairpin.findPath({70.0F, 50.0F}, {230.0F, 50.0F}, 10.0F));
    ASSERT_GE(back.size(), 4U);
    expectInside(hairpin, back);
    expectClear(back, std::vector<math::Vec2>{{140.0F, 200.0F}, {160.0F, 200.0F}}, 10.0F);
}

TEST_F(NavMeshTest, StepsIntoRoomFromPointsCloserThanTheRadiusToWalls) {
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 300.0F, 200.0F));
    mesh.addObstacle(box(140.0F, -20.0F, 20.0F, 150.0F));

    // The start hugs the left wall, and the goal lies on the right wall, where the closest point of a click beyond it lands.
    const math::Vec2 start{3.0F, 50.0F};
    const std::optional<math::Vec2> goal = mesh.getClosestPoint({320.0F, 50.0F});
    ASSERT_TRUE(goal.has_value());
    const std::vector<math::Vec2> path = copyPath(mesh.findPath(start, *goal, 10.0F));
    ASSERT_GE(path.size(), 4U);
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), *goal);
    EXPECT_NEAR(path[1].x, 10.0F, 1e-3F);
    EXPECT_NEAR(path[1].y, 50.0F, 1e-3F);
    EXPECT_NEAR(path[path.size() - 2].x, 290.0F, 1e-3F);
    EXPECT_NEAR(path[path.size() - 2].y, 50.0F, 1e-3F);
    expectInside(mesh, path);
    expectClear(std::span(path).subspan(1, path.size() - 2), std::vector<math::Vec2>{{140.0F, 130.0F}, {160.0F, 130.0F}}, 10.0F);

    // A start in a corner steps to the point the radius away from both walls.
    const std::vector<math::Vec2> cornered = copyPath(mesh.findPath({1.0F, 1.0F}, {100.0F, 100.0F}, 10.0F));
    ASSERT_GE(cornered.size(), 3U);
    EXPECT_NEAR(cornered[1].x, 10.0F, 1e-3F);
    EXPECT_NEAR(cornered[1].y, 10.0F, 1e-3F);

    // A corridor narrower than the agent leaves no room anywhere in it.
    navigation2d::NavMesh corridor;
    corridor.setBoundary(box(0.0F, 0.0F, 200.0F, 12.0F));
    EXPECT_TRUE(corridor.findPath({20.0F, 6.0F}, {180.0F, 6.0F}, 10.0F).empty());
    EXPECT_EQ(corridor.findPath({20.0F, 6.0F}, {180.0F, 6.0F}, 5.0F).size(), 2U);
}

TEST_F(NavMeshTest, KeepsAgentsOutOfGapsBetweenCornersAndWalls) {
    // The lowest corner of the rock lies 10 above the bottom wall, so only agents thinner than that pass under it.
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 800.0F, 600.0F));
    mesh.addObstacle(Polygon{{590.0F, 27.0F}, {743.0F, 10.0F}, {769.0F, 55.0F}, {660.0F, 95.0F}});
    for (const float radius : {6.0F, 12.0F}) {
        const std::vector<math::Vec2> over = copyPath(mesh.findPath({300.0F, 40.0F}, {775.0F, 30.0F}, radius));
        ASSERT_GE(over.size(), 4U);
        expectInside(mesh, over);
        EXPECT_GT(over[1].y, 95.0F);
        expectClear(over, std::vector<math::Vec2>{{660.0F, 95.0F}, {769.0F, 55.0F}}, radius);
    }
    const std::vector<math::Vec2> under = copyPath(mesh.findPath({300.0F, 40.0F}, {775.0F, 30.0F}, 4.0F));
    ASSERT_EQ(under.size(), 3U);
    expectInside(mesh, under);
    EXPECT_LT(under[1].y, 10.0F);
}

TEST_F(NavMeshTest, KeepsRandomPathsInsideTheMesh) {
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 800.0F, 600.0F));
    math::Random random(4);
    addPolygons(mesh, random, 25, 5);
    checkTriangles(mesh);

    int found = 0;
    for (int probe = 0; probe < 60; ++probe) {
        const math::Vec2 start{random.range(0.0F, 800.0F), random.range(0.0F, 600.0F)};
        const math::Vec2 goal{random.range(0.0F, 800.0F), random.range(0.0F, 600.0F)};
        if (!mesh.contains(start) || !mesh.contains(goal)) {
            continue;
        }
        const std::vector<math::Vec2> path = copyPath(mesh.findPath(start, goal));
        ASSERT_FALSE(path.empty());
        expectInside(mesh, path);
        EXPECT_GE(mesh.getPathLength() + 1e-3F, math::Vec2::distance(start, goal));
        ++found;
    }
    EXPECT_GT(found, 20);
}

TEST_F(NavMeshTest, KeepsPathsBetweenPointsNearWallsInsideTheMesh) {
    int found = 0;
    for (const int seed : {2, 5, 17, 45}) {
        navigation2d::NavMesh mesh;
        mesh.setBoundary(box(0.0F, 0.0F, 800.0F, 600.0F));
        math::Random random(static_cast<std::uint64_t>(seed));
        addPolygons(mesh, random, 20, 3 + seed % 4);
        const std::vector<math::Segment> walls = collectWalls(mesh);

        // Points pushed off a wall by less than the radius can land inside another obstacle or where the agent has no room, and those have no path.
        for (int probe = 0; probe < 40; ++probe) {
            const float radius = random.range(2.0F, 16.0F);
            const math::Vec2 start = pointNearWall(walls, random, radius);
            const math::Vec2 goal = pointNearWall(walls, random, radius);
            const std::vector<math::Vec2> path = copyPath(mesh.findPath(start, goal, radius));
            if (path.empty()) {
                continue;
            }
            EXPECT_EQ(path.front(), start);
            EXPECT_EQ(path.back(), goal);
            expectInside(mesh, path);
            ++found;
        }
    }
    EXPECT_GT(found, 100);
}

// Overlapping obstacles make many cocircular corners, where rounding could otherwise flip one diagonal back and forth forever.
TEST_F(NavMeshTest, BuildsAmongManyOverlappingSquares) {
    navigation2d::NavMesh mesh;
    mesh.setBoundary(box(0.0F, 0.0F, 4096.0F, 4096.0F));
    math::Random random(12);
    for (int obstacle = 0; obstacle < 400; ++obstacle) {
        const math::Vec2 center{random.range(100.0F, 3996.0F), random.range(100.0F, 3996.0F)};
        const float size = random.range(10.0F, 60.0F);
        mesh.addObstacle(box(center.x - size, center.y - size, size * 2.0F, size * 2.0F));
    }
    mesh.build();
    EXPECT_GT(checkTriangles(mesh), 0.0F);

    int found = 0;
    for (int probe = 0; probe < 40; ++probe) {
        const math::Vec2 start{random.range(0.0F, 4096.0F), random.range(0.0F, 4096.0F)};
        const math::Vec2 goal{random.range(0.0F, 4096.0F), random.range(0.0F, 4096.0F)};
        if (!mesh.contains(start) || !mesh.contains(goal)) {
            continue;
        }
        // Squares can close off pockets, so only the paths that exist are checked.
        const std::vector<math::Vec2> path = copyPath(mesh.findPath(start, goal, 4.0F));
        if (path.empty()) {
            continue;
        }
        expectInside(mesh, path);
        ++found;
    }
    EXPECT_GT(found, 15);
}

// Crossing squares cut each other at computed points, and an edge that ends at such a point must never count as crossing a side that ends there too, which needs an exact orientation test.
TEST_F(NavMeshTest, BuildsAmongCrossingRotatedSquares) {
    for (const int seed : {77, 220}) {
        navigation2d::NavMesh mesh;
        mesh.setBoundary(box(0.0F, 0.0F, 800.0F, 600.0F));
        math::Random random(static_cast<std::uint64_t>(seed));
        addPolygons(mesh, random, 25, 4);
        EXPECT_NO_THROW(mesh.build());
        EXPECT_GT(checkTriangles(mesh), 0.0F);
    }
}

TEST_F(NavMeshTest, RejectsBadPolygons) {
    navigation2d::NavMesh mesh;
    EXPECT_THROW(mesh.build(), std::logic_error);
    EXPECT_THROW(mesh.setBoundary(Polygon{{0.0F, 0.0F}, {1.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(mesh.addObstacle(Polygon{{0.0F, 0.0F}, {std::nanf(""), 0.0F}, {1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(mesh.setObstacle(7, box(0.0F, 0.0F, 1.0F, 1.0F)), std::out_of_range);
}

} // namespace haylen
