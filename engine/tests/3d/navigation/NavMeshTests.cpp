#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "3d/navigation/NavMeshFile.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Random.hpp"
#include "support/DamagedNavMeshes.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::navigation3d {

// Bakes small worlds of boxes and checks what their meshes answer: paths around walls and through doors, steps and ledges, areas with costs, rays, random points, links, obstacles and rebuilds.
class NavMesh3DTest : public ::testing::Test {
  protected:
    // A floor of the side around the origin, as two triangles that face up.
    static void addFloor(SourceGeometry& source, float side, float height = 0.0F, std::string_view area = SourceGeometry::kGround) {
        const float half = side * 0.5F;
        const std::vector<math::Vec3> corners{{-half, height, -half}, {-half, height, half}, {half, height, half}, {half, height, -half}};
        const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};
        source.addMesh(corners, indices, math::Mat4::identity(), area);
    }

    static void addWall(SourceGeometry& source, math::Vec3 center, math::Vec3 halfExtents) {
        source.addBox({.center = center, .rotation = math::Quat::identity(), .halfExtents = halfExtents});
    }

    // A floor of 30 meters split by a wall across x = 0 with a door of the width around z = 0.
    [[nodiscard]] static SourceGeometry splitRoom(float door) {
        SourceGeometry source;
        addFloor(source, 30.0F);
        const float wing = (30.0F - door) * 0.25F;
        addWall(source, {0.0F, 1.5F, -door * 0.5F - wing}, {0.25F, 1.5F, wing});
        addWall(source, {0.0F, 1.5F, door * 0.5F + wing}, {0.25F, 1.5F, wing});
        return source;
    }

    [[nodiscard]] static BuildSettings settings() {
        BuildSettings value;
        value.tileSize = 9.6F;
        value.agentRadius = 0.4F;
        return value;
    }

    [[nodiscard]] static std::string errorOf(const auto& call) {
        try {
            call();
        } catch (const std::exception& exception) {
            return exception.what();
        }

        return {};
    }
};

TEST_F(NavMesh3DTest, BakesAFloorIntoTilesAndWalksStraightAcrossIt) {
    SourceGeometry source;
    addFloor(source, 30.0F);
    const NavMesh mesh = NavMesh::build(source, settings());

    EXPECT_GT(mesh.getTileCount(), 4U);
    EXPECT_GT(mesh.getPolygonCount(), 0U);

    const std::optional<NavMesh::Path> path = mesh.findPath({-12.0F, 0.0F, -12.0F}, {12.0F, 0.0F, 12.0F});
    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->partial);
    ASSERT_EQ(path->points.size(), 2U);
    EXPECT_NEAR(path->length, std::sqrt(2.0F) * 24.0F, 0.2F);
}

TEST_F(NavMesh3DTest, WalksStraightAcrossTheCornersOfWideTiles) {
    // A search that weighs steps by the middles of the edges of wide polygons would bend this path to the corners of the tiles beside it.
    BuildSettings wide = settings();
    wide.tileSize = 12.0F;
    SourceGeometry source;
    addFloor(source, 30.0F);
    const NavMesh mesh = NavMesh::build(source, wide);

    const std::optional<NavMesh::Path> path = mesh.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F});
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->points.size(), 2U);
    EXPECT_NEAR(path->length, 20.0F, 0.01F);
}

TEST_F(NavMesh3DTest, GoesThroughADoorWideEnoughForTheAgent) {
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());

    const std::optional<NavMesh::Path> path = mesh.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F});
    ASSERT_TRUE(path.has_value());
    EXPECT_FALSE(path->partial);
    ASSERT_GE(path->points.size(), 3U);

    // The path turns at the door and never crosses the wall anywhere else.
    bool throughDoor = false;
    for (std::size_t index = 1; index < path->points.size(); ++index) {
        const math::Vec3 from = path->points[index - 1];
        const math::Vec3 to = path->points[index];
        if ((from.x < 0.0F) != (to.x < 0.0F)) {
            const float crossing = from.z + (to.z - from.z) * (0.0F - from.x) / (to.x - from.x);
            throughDoor = std::fabs(crossing) < 1.0F;
        }
    }
    EXPECT_TRUE(throughDoor);
}

TEST_F(NavMesh3DTest, AnswersABlockedPathWhenTheDoorIsNarrowerThanTheAgent) {
    const NavMesh mesh = NavMesh::build(splitRoom(0.5F), settings());

    const std::optional<NavMesh::Path> path = mesh.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F});
    ASSERT_TRUE(path.has_value());
    EXPECT_TRUE(path->partial);
    EXPECT_LT(path->points.back().x, 0.0F);
}

TEST_F(NavMesh3DTest, LeavesATileWithoutGroundOutAndGoesAroundIt) {
    // Two floors leave the middle tile of the bottom row without ground, and the tiles around it join past it.
    SourceGeometry source;
    const std::vector<math::Vec3> corners{{-14.4F, 0.0F, -4.8F}, {-14.4F, 0.0F, 14.4F}, {14.4F, 0.0F, 14.4F}, {14.4F, 0.0F, -4.8F}, {-14.4F, 0.0F, -14.4F}, {-14.4F, 0.0F, -4.8F}, {-4.8F, 0.0F, -4.8F}, {-4.8F, 0.0F, -14.4F}, {4.8F, 0.0F, -14.4F}, {4.8F, 0.0F, -4.8F}, {14.4F, 0.0F, -4.8F}, {14.4F, 0.0F, -14.4F}};
    const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};
    source.addMesh(corners, indices);
    const NavMesh mesh = NavMesh::build(source, settings());
    EXPECT_EQ(mesh.getTileCount(), 8U);

    EXPECT_FALSE(mesh.findNearest({0.0F, 0.0F, -10.0F}, {.searchExtents = {1.0F, 1.0F, 1.0F}}).has_value());
    EXPECT_FALSE(mesh.findPath({-10.0F, 0.0F, -10.0F}, {0.0F, 0.0F, -10.0F}).has_value());

    const std::optional<NavMesh::Path> around = mesh.findPath({-10.0F, 0.0F, -10.0F}, {10.0F, 0.0F, -10.0F});
    ASSERT_TRUE(around.has_value());
    EXPECT_FALSE(around->partial);
    EXPECT_GT(around->length, 22.0F);
}

TEST_F(NavMesh3DTest, ClimbsStepsUpToItsClimbAndNoHigherLedge) {
    SourceGeometry source;
    addFloor(source, 30.0F);
    for (int step = 0; step < 4; ++step) {
        const float top = 0.3F * static_cast<float>(step + 1);
        addWall(source, {2.0F + static_cast<float>(step), top * 0.5F, 0.0F}, {0.5F, top * 0.5F, 3.0F});
    }
    addWall(source, {-8.0F, 0.75F, 0.0F}, {2.0F, 0.75F, 3.0F});
    const NavMesh mesh = NavMesh::build(source, settings());

    const std::optional<NavMesh::Location> stairs = mesh.findNearest({5.0F, 1.2F, 0.0F}, {.searchExtents = {0.2F, 0.3F, 0.2F}});
    ASSERT_TRUE(stairs.has_value());
    const std::optional<NavMesh::Path> up = mesh.findPath({0.0F, 0.0F, 0.0F}, stairs->position);
    ASSERT_TRUE(up.has_value());
    EXPECT_FALSE(up->partial);

    const std::optional<NavMesh::Location> ledge = mesh.findNearest({-8.0F, 1.5F, 0.0F}, {.searchExtents = {0.2F, 0.3F, 0.2F}});
    ASSERT_TRUE(ledge.has_value());
    const std::optional<NavMesh::Path> blocked = mesh.findPath({0.0F, 0.0F, 0.0F}, ledge->position);
    ASSERT_TRUE(blocked.has_value());
    EXPECT_TRUE(blocked->partial);
}

TEST_F(NavMesh3DTest, WeighsAreasByTheCostsOfTheFilterAndSkipsExcludedOnes) {
    SourceGeometry source;
    addFloor(source, 30.0F, 0.0F, "grass");
    source.addVolume({.outline = {{-15.0F, 0.0F, 6.0F}, {-15.0F, 0.0F, 9.0F}, {15.0F, 0.0F, 9.0F}, {15.0F, 0.0F, 6.0F}}, .bottom = -1.0F, .top = 1.0F, .area = "road"});
    BuildSettings wide = settings();
    wide.tileSize = 12.0F;
    const NavMesh mesh = NavMesh::build(source, wide);
    ASSERT_EQ(mesh.getArea("grass"), 2U);
    ASSERT_EQ(mesh.getArea("road"), 3U);

    const std::optional<NavMesh::Path> straight = mesh.findPath({-12.0F, 0.0F, 0.0F}, {12.0F, 0.0F, 0.0F});
    ASSERT_TRUE(straight.has_value());
    EXPECT_NEAR(straight->length, 24.0F, 0.2F);

    // A ray that shortens the path through grass costs more than the road, so the search keeps to the road.
    QueryFilter slowGrass;
    slowGrass.setCost(mesh.getArea("grass"), 10.0F);
    const std::optional<NavMesh::Path> byRoad = mesh.findPath({-12.0F, 0.0F, 0.0F}, {12.0F, 0.0F, 0.0F}, {.filter = &slowGrass});
    ASSERT_TRUE(byRoad.has_value());
    EXPECT_GT(byRoad->length, 28.0F);

    QueryFilter noRoad;
    noRoad.setExcluded(mesh.getArea("road"), true);
    EXPECT_FALSE(mesh.findNearest({0.0F, 0.0F, 7.5F}, {.filter = &noRoad, .searchExtents = {0.5F, 1.0F, 0.5F}}).has_value());
}

TEST_F(NavMesh3DTest, CastsRaysAlongTheSurfaceToTheWalls) {
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());

    const std::optional<NavMesh::RayHit> wall = mesh.raycast({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F});
    ASSERT_TRUE(wall.has_value());
    EXPECT_TRUE(wall->hit);
    // The wall stands a radius of the agent away from its face, within a voxel.
    EXPECT_NEAR(wall->position.x, -0.65F, 0.31F);
    EXPECT_NEAR(wall->normal.x, -1.0F, 0.01F);

    const std::optional<NavMesh::RayHit> open = mesh.raycast({-10.0F, 0.0F, 8.0F}, {-2.0F, 0.0F, 8.0F});
    ASSERT_TRUE(open.has_value());
    EXPECT_FALSE(open->hit);
}

TEST_F(NavMesh3DTest, FindsTheSameRandomPointsForTheSameSeed) {
    SourceGeometry source;
    addFloor(source, 30.0F);
    const NavMesh mesh = NavMesh::build(source, settings());

    math::Random first(7);
    math::Random second(7);
    for (int index = 0; index < 50; ++index) {
        const std::optional<NavMesh::Location> a = mesh.findRandomPoint(first);
        const std::optional<NavMesh::Location> b = mesh.findRandomPoint(second);
        ASSERT_TRUE(a.has_value());
        ASSERT_TRUE(b.has_value());
        EXPECT_EQ(a->position, b->position);
        EXPECT_LE(std::fabs(a->position.x), 15.0F);
        EXPECT_LE(std::fabs(a->position.z), 15.0F);
    }

    const std::optional<NavMesh::Location> near = mesh.findRandomPointAround({5.0F, 0.0F, 5.0F}, 2.0F, first);
    ASSERT_TRUE(near.has_value());
    EXPECT_LE(math::Vec3::distance(near->position, {5.0F, 0.0F, 5.0F}), 2.0F);
}

TEST_F(NavMesh3DTest, WritesAndReadsBackTheSameMesh) {
    SourceGeometry source = splitRoom(2.0F);
    source.addLink({.start = {-5.0F, 0.0F, -5.0F}, .end = {5.0F, 0.0F, -5.0F}, .kind = Link::Kind::Teleport});
    const NavMesh mesh = NavMesh::build(source, settings());
    const std::vector<std::uint8_t> bytes = mesh.encode();

    const NavMesh read = NavMesh::decode(bytes, "levels/room.navmesh");
    EXPECT_EQ(read.encode(), bytes);
    EXPECT_EQ(read.getTileCount(), mesh.getTileCount());
    EXPECT_EQ(read.getPolygonCount(), mesh.getPolygonCount());
    EXPECT_EQ(read.getLinkCount(), 1U);

    BuildSettings carved = settings();
    carved.obstacles = true;
    const std::vector<std::uint8_t> layers = NavMesh::build(source, carved).encode();
    EXPECT_EQ(NavMesh::decode(layers, "levels/room.navmesh").encode(), layers);
}

TEST_F(NavMesh3DTest, RefusesEveryCutOfAFileWithTheSentenceOfADamagedFile) {
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());
    const std::vector<std::uint8_t> bytes = mesh.encode();

    for (std::size_t size = 0; size < bytes.size(); size += 7) {
        const std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
        EXPECT_EQ(errorOf([&cut] { static_cast<void>(NavMesh::decode(cut, "levels/room.navmesh")); }), "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");
    }
}

TEST_F(NavMesh3DTest, RefusesAPolygonWhoseDetailMarksNoEdgeOfItsOutline) {
    // The closest point of a polygon follows the edges of its outline, so a file whose detail triangles mark none of them would leave that search without an edge.
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());
    NavMeshFile::Contents contents = NavMeshFile::decode(mesh.encode(), "levels/room.navmesh");
    ASSERT_FALSE(contents.meshes.empty());
    TileMesh& tile = contents.meshes.front();
    const std::uint32_t first = tile.detailMeshes[2];
    const std::uint32_t count = tile.detailMeshes[3];
    for (std::uint32_t triangle = first; triangle < first + count; ++triangle) {
        tile.detailTriangles[static_cast<std::size_t>(triangle) * 4 + 3] = 0;
    }

    const std::vector<std::uint8_t> bytes = NavMeshFile::encode(contents);
    EXPECT_EQ(errorOf([&bytes] { static_cast<void>(NavMesh::decode(bytes, "levels/room.navmesh")); }), "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");
}

TEST_F(NavMesh3DTest, RefusesPointsSoFarAwayThatTheirDistancesOverflow) {
    // The square of the distance to a point this far away is past the floats, so a search for the closest edge would find none.
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());
    NavMeshFile::Contents contents = NavMeshFile::decode(mesh.encode(), "levels/room.navmesh");
    ASSERT_FALSE(contents.meshes.empty());
    contents.meshes.front().min[0] = -1.0e30F;
    const std::vector<std::uint8_t> bytes = NavMeshFile::encode(contents);
    EXPECT_EQ(errorOf([&bytes] { static_cast<void>(NavMesh::decode(bytes, "levels/room.navmesh")); }), "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");

    SourceGeometry far;
    EXPECT_EQ(errorOf([&far] { far.addSphere({.center = {2.0e12F, 0.0F, 0.0F}, .radius = 1.0F}); }), "Every center of a sphere of the geometry of a navigation mesh must be a finite number within a trillion meters of the origin.");
}

TEST_F(NavMesh3DTest, ReadsFlippedBytesOnlyIntoMeshesThatStillAnswerQueries) {
    const NavMesh mesh = NavMesh::build(splitRoom(2.0F), settings());
    const std::vector<std::uint8_t> bytes = mesh.encode();

    math::Random random(11);
    for (int attempt = 0; attempt < 400; ++attempt) {
        std::vector<std::uint8_t> damaged = bytes;
        damaged[static_cast<std::size_t>(random.range(0, static_cast<int>(damaged.size()) - 1))] ^= static_cast<std::uint8_t>(1U << static_cast<unsigned>(random.range(0, 7)));
        try {
            const NavMesh read = NavMesh::decode(damaged, "levels/room.navmesh");
            static_cast<void>(read.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F}));
            static_cast<void>(read.getTriangles());
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");
        }
    }
}

TEST_F(NavMesh3DTest, ReadsLayersWhoseDamagedVoxelsBreakTheRemovalOfCorners) {
    // The fuzzer found these inputs. The first leaves more edges around one corner of an obstacle layer than the removal of that corner keeps, which now keeps the corner, so the mesh reads with every triangle inside its bounds. The others leave a hole of fewer than three corners where a corner was removed, which the removal filled with corners taken from numbers that nothing wrote and now leaves unfilled, and the check of the polygons that their damaged layers make refuses them. The sanitizers see a filling of such a hole only when those numbers point past the mesh.
    const std::vector<std::pair<std::vector<std::uint8_t>, bool>> inputs{
        {{0x2a, 0x13, 0xca, 0x21, 0x13, 0xda}, false},
        {{0x62, 0x0f, 0x7e, 0xff, 0x3b, 0x3a, 0xba}, true},
        {{0x0a, 0xff, 0x0a, 0x0e, 0x4a, 0x8a, 0x3a}, true},
    };

    for (std::size_t index = 0; index < inputs.size(); ++index) {
        SCOPED_TRACE(index);
        const auto& [input, refused] = inputs[index];
        const std::vector<std::uint8_t> bytes = test::DamagedNavMeshes::damage(input);
        std::optional<NavMesh> read;
        const std::string error = errorOf([&bytes, &read] { read.emplace(NavMesh::decode(bytes, "levels/room.navmesh")); });
        if (refused) {
            EXPECT_EQ(error, "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");
            continue;
        }

        ASSERT_EQ(error, "");
        ASSERT_TRUE(read.has_value());
        const math::Box& bounds = read->getBounds();
        const NavMesh::Triangles triangles = read->getTriangles();
        EXPECT_FALSE(triangles.corners.empty());
        for (const math::Vec3& corner : triangles.corners) {
            EXPECT_TRUE(corner.x >= bounds.min.x && corner.x <= bounds.max.x && corner.z >= bounds.min.z && corner.z <= bounds.max.z);
        }
    }
}

TEST_F(NavMesh3DTest, RefusesLayersWhoseVoxelsMakePolygonsThatNoPathCrosses) {
    // The fuzzer found these inputs, which damage the voxels of obstacle layers into polygons that a ray walks through without end.
    const std::vector<std::vector<std::uint8_t>> inputs{
        {0xf0, 0x05, 0xb0, 0x23, 0x50, 0x23, 0x23, 0x50, 0x94, 0xda, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x1e, 0x2f, 0xb0, 0x23, 0xb0, 0x23, 0x94, 0xb0, 0x50, 0x01, 0x14, 0xb0, 0x50, 0xb1},
    };

    for (const std::vector<std::uint8_t>& input : inputs) {
        const std::vector<std::uint8_t> bytes = test::DamagedNavMeshes::damage(input);
        EXPECT_EQ(errorOf([&bytes] { static_cast<void>(NavMesh::decode(bytes, "levels/room.navmesh")); }), "The navigation mesh \"levels/room.navmesh\" is damaged or was not made by this version of Haylen.");
    }
}

TEST_F(NavMesh3DTest, RefusesSettingsOutOfTheirRangeWithTheirName) {
    BuildSettings wrong = settings();
    wrong.agentMaxSlope = 2.0F;
    EXPECT_EQ(errorOf([&wrong] { wrong.validate(); }), "The setting \"agentMaxSlope\" of a navigation mesh must be an angle in radians from 0 up to, but not including, half of pi.");

    wrong = settings();
    wrong.obstacles = true;
    wrong.tileSize = 90.0F;
    EXPECT_EQ(errorOf([&wrong] { wrong.validate(); }), "The setting \"tileSize\" of a navigation mesh must hold from 8 to 245 voxels of its \"cellSize\" on a side, but it holds 300.");

    EXPECT_EQ(errorOf([] { static_cast<void>(NavMesh::build(SourceGeometry{}, BuildSettings{})); }), "The geometry of a navigation mesh holds no triangles, so there is nothing to bake.");
}

TEST_F(NavMesh3DTest, BakesTheSameMeshInJobsAsOnOneThread) {
    test::EngineFixture fixture;
    const auto source = std::make_shared<SourceGeometry>(splitRoom(2.0F));
    const std::vector<std::uint8_t> serial = NavMesh::build(*source, settings()).encode();

    std::optional<core::JobSystem::Result<NavMesh>> result;
    NavMesh::buildAsync(fixture.engine().getJobs(), source, settings(), [&result](core::JobSystem::Result<NavMesh> value) { result = std::move(value); });
    ASSERT_TRUE(fixture.frameUntil([&result] { return result.has_value(); }));
    ASSERT_TRUE(result->isOk()) << result->error;
    EXPECT_EQ(result->value->encode(), serial);
}

TEST_F(NavMesh3DTest, BakesInJobsMeshesWhoseOnlyColumnFinishesBeforeTheJobThatPostedIt) {
    // A mesh of one column hands its state over as soon as its one column bakes, often before the job that posted the column returns, which must not read the state again. The floor is 6 meters on a side, so what stays walkable once the bake drops its ledges and the radius of the agent is larger than the smallest region that the bake keeps, and every bake makes its one tile.
    test::EngineFixture fixture;
    auto source = std::make_shared<SourceGeometry>();
    addFloor(*source, 6.0F);
    const std::shared_ptr<const SourceGeometry> floor = source;

    constexpr std::size_t kBakes = 32;
    std::vector<std::string> outcomes;
    for (std::size_t bake = 0; bake < kBakes; ++bake) {
        // clang-format off
        NavMesh::buildAsync(fixture.engine().getJobs(), floor, settings(), [&outcomes](core::JobSystem::Result<NavMesh> value) {
            outcomes.push_back(value.isOk() ? std::to_string(value.value->getTileCount()) + " tiles" : value.error);
        });
        // clang-format on
    }

    ASSERT_TRUE(fixture.frameUntil([&outcomes] { return outcomes.size() == kBakes; }));
    EXPECT_EQ(outcomes, std::vector<std::string>(kBakes, "1 tiles"));
}

TEST_F(NavMesh3DTest, CarvesTheMeshAroundAnObstacleAndOpensItAgainWhenItMoves) {
    BuildSettings carved = settings();
    carved.obstacles = true;
    NavMesh mesh = NavMesh::build(splitRoom(2.0F), carved);
    ASSERT_FALSE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);

    // A box in the door blocks the way until it moves aside.
    const std::uint32_t crate = mesh.addObstacle(Obstacle::box({0.0F, 0.5F, 0.0F}, {0.8F, 0.5F, 1.2F}));
    EXPECT_FALSE(mesh.isUpToDate());
    mesh.update(nullptr);
    EXPECT_TRUE(mesh.isUpToDate());
    EXPECT_TRUE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);

    mesh.setObstacle(crate, Obstacle::box({-6.0F, 0.5F, 6.0F}, {0.8F, 0.5F, 1.2F}));
    mesh.update(nullptr);
    EXPECT_FALSE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);
    EXPECT_FALSE(mesh.findNearest({-6.0F, 0.0F, 6.0F}, {.searchExtents = {0.2F, 1.0F, 0.2F}}).has_value());

    EXPECT_TRUE(mesh.removeObstacle(crate));
    mesh.update(nullptr);
    EXPECT_TRUE(mesh.findNearest({-6.0F, 0.0F, 6.0F}, {.searchExtents = {0.2F, 1.0F, 0.2F}}).has_value());
}

TEST_F(NavMesh3DTest, TurnsABoxObstacleByItsYawAroundTheUpAxis) {
    BuildSettings carved = settings();
    carved.obstacles = true;
    SourceGeometry source;
    addFloor(source, 30.0F);
    NavMesh mesh = NavMesh::build(source, carved);

    // A long box turned a quarter turn lies along z, and turned by a positive yaw its end along x moves toward negative z.
    mesh.addObstacle(Obstacle::box({0.0F, 0.5F, 0.0F}, {4.0F, 0.5F, 0.3F}, 0.5F));
    mesh.update(nullptr);
    const math::Vec3 end = math::Quat::fromAxisAngle(math::Vec3::up(), 0.5F).rotate({3.5F, 0.0F, 0.0F});
    EXPECT_LT(end.z, 0.0F);
    EXPECT_FALSE(mesh.findNearest(end, {.searchExtents = {0.1F, 1.0F, 0.1F}}).has_value());
    EXPECT_TRUE(mesh.findNearest({end.x, 0.0F, -end.z}, {.searchExtents = {0.1F, 1.0F, 0.1F}}).has_value());
}

TEST_F(NavMesh3DTest, RebuildsTheTilesOfAWallThatFellInJobs) {
    test::EngineFixture fixture;
    NavMesh mesh = NavMesh::build(splitRoom(0.5F), settings());
    ASSERT_TRUE(mesh.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F})->partial);

    // The wall falls, so the tiles along it bake again from the floor alone.
    auto open = std::make_shared<SourceGeometry>();
    addFloor(*open, 30.0F);
    mesh.rebuild(open, {{-1.0F, -1.0F, -15.0F}, {1.0F, 3.0F, 15.0F}});
    bool done = false;
    std::string failure;
    // clang-format off
    mesh.whenUpToDate([&done, &failure](std::string error) {
        done = true;
        failure = std::move(error);
    });
    // clang-format on
    mesh.update(&fixture.engine().getJobs());
    ASSERT_TRUE(fixture.frameUntil([&done] { return done; }));
    EXPECT_EQ(failure, "");
    EXPECT_FALSE(mesh.findPath({-10.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 8.0F})->partial);
}

TEST_F(NavMesh3DTest, CrossesAGapByALinkThatGoesOneWay) {
    SourceGeometry source;
    addFloor(source, 30.0F);
    addWall(source, {0.0F, 1.5F, 0.0F}, {0.25F, 1.5F, 15.0F});
    source.addLink({.start = {-1.0F, 0.0F, 0.0F}, .end = {1.0F, 0.0F, 0.0F}, .bidirectional = false, .kind = Link::Kind::Jump});
    const NavMesh mesh = NavMesh::build(source, settings());

    const std::optional<NavMesh::Path> forward = mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F});
    ASSERT_TRUE(forward.has_value());
    EXPECT_FALSE(forward->partial);
    EXPECT_NE(std::find(forward->links.begin(), forward->links.end(), 1U), forward->links.end());

    const std::optional<NavMesh::Path> back = mesh.findPath({10.0F, 0.0F, 0.0F}, {-10.0F, 0.0F, 0.0F});
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->partial);
}

TEST_F(NavMesh3DTest, AddsAndRemovesLinksAtRunTime) {
    SourceGeometry source;
    addFloor(source, 30.0F);
    addWall(source, {0.0F, 1.5F, 0.0F}, {0.25F, 1.5F, 15.0F});
    NavMesh mesh = NavMesh::build(source, settings());
    ASSERT_TRUE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);

    const std::uint32_t ladder = mesh.addLink({.start = {-1.0F, 0.0F, 3.0F}, .end = {1.0F, 0.0F, 3.0F}, .kind = Link::Kind::Ladder});
    mesh.update(nullptr);
    EXPECT_FALSE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);
    EXPECT_EQ(mesh.getLink(ladder)->kind, Link::Kind::Ladder);

    EXPECT_TRUE(mesh.removeLink(ladder));
    mesh.update(nullptr);
    EXPECT_TRUE(mesh.findPath({-10.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F})->partial);
}

} // namespace haylen::navigation3d
