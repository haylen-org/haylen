#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/3d/navigation/Crowd.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/core/JobSystem.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::navigation3d {

// Steps crowds on baked floors: many agents that cross a plaza, the same steps on one thread and on many, agents that go around an obstacle that moves and that cross links by their kind.
class Crowd3DTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 0.1F;

    static void addFloor(SourceGeometry& source, float side) {
        const float half = side * 0.5F;
        const std::vector<math::Vec3> corners{{-half, 0.0F, -half}, {-half, 0.0F, half}, {half, 0.0F, half}, {half, 0.0F, -half}};
        const std::vector<std::uint32_t> indices{0, 1, 2, 0, 2, 3};
        source.addMesh(corners, indices);
    }

    static void addWall(SourceGeometry& source, math::Vec3 center, math::Vec3 halfExtents) {
        source.addBox({.center = center, .rotation = math::Quat::identity(), .halfExtents = halfExtents});
    }

    [[nodiscard]] static NavMesh plaza(bool obstacles = false) {
        SourceGeometry source;
        addFloor(source, 140.0F);
        for (int pillar = 0; pillar < 5; ++pillar) {
            addWall(source, {0.0F, 1.5F, -36.0F + static_cast<float>(pillar) * 18.0F}, {1.5F, 1.5F, 1.5F});
        }

        BuildSettings settings;
        settings.agentRadius = 0.4F;
        settings.obstacles = obstacles;
        settings.tileSize = obstacles ? 19.2F : 32.0F;
        return NavMesh::build(source, settings);
    }

    // Places agents on a tight grid on the left of the plaza, each heading for its own place on a grid of two meters on the right. The agents in front go farthest, and the agents that arrived leave gaps wide enough for the others to pass.
    static std::vector<Crowd::AgentId> fillPlaza(Crowd& crowd, int count) {
        std::vector<Crowd::AgentId> ids;
        const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        for (int index = 0; index < count; ++index) {
            const int column = index % columns;
            const int row = index / columns;
            const math::Vec3 start{-68.0F + static_cast<float>(column) * 1.3F, 0.0F, -20.0F + static_cast<float>(row) * 1.3F};
            const math::Vec3 target{4.0F + static_cast<float>(column) * 2.0F, 0.0F, -31.0F + static_cast<float>(row) * 2.0F};
            const Crowd::AgentId id = crowd.addAgent({.position = start, .radius = 0.4F, .maxSpeed = 3.5F});
            EXPECT_TRUE(crowd.setTarget(id, target));
            ids.push_back(id);
        }

        return ids;
    }

    [[nodiscard]] static std::size_t countArrived(const Crowd& crowd, const std::vector<Crowd::AgentId>& ids, float within) {
        std::size_t arrived = 0;
        for (const Crowd::AgentId id : ids) {
            const std::optional<math::Vec3> target = crowd.getTarget(id);
            if (target && math::Vec3::distance(crowd.getPosition(id), *target) < within) {
                ++arrived;
            }
        }

        return arrived;
    }
};

TEST_F(Crowd3DTest, CrossesAPlazaWithAThousandAgentsWithoutAllocatingAsItSteps) {
    test::EngineFixture fixture;
    const NavMesh mesh = plaza();
    Crowd crowd(mesh, {.maxAgents = 1000, .maxAgentRadius = 0.4F});
    const std::vector<Crowd::AgentId> ids = fillPlaza(crowd, 1000);
    core::JobSystem& jobs = fixture.engine().getJobs();

    for (int step = 0; step < 20; ++step) {
        crowd.step(kStep, &jobs);
    }

    // Once every agent has its path, a step on one thread allocates nothing, and over the job system only the shares of the job system allocate, the same in every step.
    {
        const test::AllocationTracker tracker;
        crowd.step(kStep, nullptr);
        EXPECT_EQ(tracker.getTotal(), 0U);
    }
    std::size_t earlier = 0;
    {
        const test::AllocationTracker tracker;
        crowd.step(kStep, &jobs);
        earlier = tracker.getTotal();
    }
    for (int step = 0; step < 50; ++step) {
        crowd.step(kStep, &jobs);
    }
    {
        const test::AllocationTracker tracker;
        crowd.step(kStep, &jobs);
        EXPECT_LE(tracker.getTotal(), earlier);
    }

    // The longest walk, about 100 meters at 3.5 meters a second, takes 30 seconds alone, and the whole crowd, which queues into the gaps between the agents that arrived, gets there within three times that.
    int steps = 70;
    for (; steps < 900 && countArrived(crowd, ids, 1.0F) < ids.size(); ++steps) {
        crowd.step(kStep, &jobs);
    }

    for (const Crowd::AgentId id : ids) {
        EXPECT_EQ(crowd.getState(id), Crowd::AgentState::Walking);
        EXPECT_TRUE(crowd.getPosition(id).isFinite());
    }
    EXPECT_EQ(countArrived(crowd, ids, 1.0F), ids.size());
    EXPECT_LT(steps, 900);
}

TEST_F(Crowd3DTest, StepsTheSameOnOneThreadAsOnMany) {
    test::EngineFixture fixture;
    const NavMesh mesh = plaza();
    Crowd serial(mesh, {.maxAgents = 300});
    Crowd parallel(mesh, {.maxAgents = 300});
    fillPlaza(serial, 300);
    fillPlaza(parallel, 300);

    for (int step = 0; step < 120; ++step) {
        serial.step(kStep, nullptr);
        parallel.step(kStep, &fixture.engine().getJobs());
    }

    std::vector<float> serialPositions(serial.getAgentCount() * 3);
    std::vector<float> parallelPositions(parallel.getAgentCount() * 3);
    serial.readPositions(serialPositions);
    parallel.readPositions(parallelPositions);
    EXPECT_EQ(serialPositions, parallelPositions);
}

TEST_F(Crowd3DTest, KeepsWalkingTowardTheGoalAlongAStablePath) {
    const NavMesh mesh = plaza();
    Crowd crowd(mesh);
    const Crowd::AgentId id = crowd.addAgent({.position = {-20.0F, 0.0F, -36.0F}});
    const math::Vec3 goal{20.0F, 0.0F, -36.0F};
    ASSERT_TRUE(crowd.setTarget(id, goal));

    // A lone agent around a pillar gets closer to its goal at every step, and the path from where it stands keeps the same turns.
    const std::optional<NavMesh::Path> first = mesh.findPath(crowd.getPosition(id), goal);
    ASSERT_TRUE(first.has_value());
    ASSERT_GE(first->points.size(), 3U);
    float distance = math::Vec3::distance(crowd.getPosition(id), goal);
    for (int step = 0; step < 200 && distance > 0.3F; ++step) {
        crowd.step(kStep);
        const float next = math::Vec3::distance(crowd.getPosition(id), goal);
        EXPECT_LE(next, distance + 1e-4F);
        distance = next;
    }

    EXPECT_LT(distance, 0.3F);
    EXPECT_EQ(mesh.findPath({-20.0F, 0.0F, -36.0F}, goal)->points, first->points);
}

TEST_F(Crowd3DTest, GoesAroundAnObstacleWhoseTilesRebuildAsItMoves) {
    test::EngineFixture fixture;
    core::JobSystem& jobs = fixture.engine().getJobs();
    auto mesh = std::make_shared<NavMesh>(plaza(true));
    Crowd crowd(*mesh, {.maxAgents = 20});
    std::vector<Crowd::AgentId> ids;
    for (int index = 0; index < 20; ++index) {
        const float z = -4.0F + static_cast<float>(index % 5) * 2.0F;
        const float x = -30.0F - static_cast<float>(index / 5) * 2.0F;
        ids.push_back(crowd.addAgent({.position = {x, 0.0F, z}, .radius = 0.4F}));
        crowd.setTarget(ids.back(), {x + 60.0F, 0.0F, z});
    }

    // A truck parks across the way, and later rolls on to the right side of the plaza, where nobody walks yet.
    const Obstacle parked = Obstacle::box({-10.0F, 1.0F, 0.0F}, {1.5F, 1.0F, 6.0F});
    const Obstacle moved = Obstacle::box({15.0F, 1.0F, 0.0F}, {1.5F, 1.0F, 6.0F});
    const std::uint32_t truck = mesh->addObstacle(parked);
    bool rolled = false;
    for (int frame = 0; frame < 600 && countArrived(crowd, ids, 1.0F) < ids.size(); ++frame) {
        if (!rolled && frame == 60) {
            mesh->setObstacle(truck, moved);
            rolled = true;
        }

        mesh->update(&jobs);
        crowd.step(kStep, &jobs);
        fixture.frames(1);

        // Once the tiles of the truck are in place, no agent stands on it.
        const Obstacle& where = rolled ? moved : parked;
        for (const Crowd::AgentId id : ids) {
            const math::Vec3 position = crowd.getPosition(id);
            const bool inside = std::fabs(position.x - where.position.x) < where.halfExtents.x && std::fabs(position.z - where.position.z) < where.halfExtents.z;
            EXPECT_FALSE(mesh->isUpToDate() && frame > 70 && inside) << "Agent " << id << " stands on the truck at frame " << frame << ".";
        }
    }

    EXPECT_EQ(countArrived(crowd, ids, 1.0F), ids.size());
}

TEST_F(Crowd3DTest, CrossesLinksByTheirKindAndReportsEachEntry) {
    SourceGeometry source;
    addFloor(source, 40.0F);
    addWall(source, {0.0F, 1.5F, 0.0F}, {0.25F, 1.5F, 20.0F});
    source.addLink({.start = {-1.0F, 0.0F, -10.0F}, .end = {1.0F, 0.0F, -10.0F}, .kind = Link::Kind::Teleport});
    source.addLink({.start = {-1.0F, 0.0F, 0.0F}, .end = {1.0F, 0.0F, 0.0F}, .kind = Link::Kind::Ladder});
    source.addLink({.start = {-1.0F, 0.0F, 10.0F}, .end = {1.0F, 0.0F, 10.0F}, .kind = Link::Kind::Jump});
    BuildSettings settings;
    settings.agentRadius = 0.4F;
    const NavMesh mesh = NavMesh::build(source, settings);
    Crowd crowd(mesh);

    const Crowd::AgentId teleporting = crowd.addAgent({.position = {-6.0F, 0.0F, -10.0F}});
    const Crowd::AgentId climbing = crowd.addAgent({.position = {-6.0F, 0.0F, 0.0F}, .climbSpeed = 0.5F});
    const Crowd::AgentId waiting = crowd.addAgent({.position = {-6.0F, 0.0F, 10.0F}, .linkTraversal = Crowd::LinkTraversal::Manual});
    crowd.setTarget(teleporting, {6.0F, 0.0F, -10.0F});
    crowd.setTarget(climbing, {6.0F, 0.0F, 0.0F});
    crowd.setTarget(waiting, {6.0F, 0.0F, 10.0F});

    std::vector<Crowd::LinkEntry> entries;
    int climbSteps = 0;
    for (int step = 0; step < 100; ++step) {
        crowd.step(kStep);
        entries.insert(entries.end(), crowd.getLinkEntries().begin(), crowd.getLinkEntries().end());
        climbSteps += crowd.getState(climbing) == Crowd::AgentState::OnLink ? 1 : 0;
    }

    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].kind, Link::Kind::Teleport);
    EXPECT_EQ(entries[0].agent, teleporting);
    EXPECT_GT(crowd.getPosition(teleporting).x, 5.0F);

    // A ladder of two meters at half a meter a second takes four seconds besides the walk to its start.
    EXPECT_GE(climbSteps, 40);
    EXPECT_GT(crowd.getPosition(climbing).x, 5.0F);

    // A manual crossing waits at the start of the link until the app lets it go.
    EXPECT_EQ(crowd.getState(waiting), Crowd::AgentState::OnLink);
    EXPECT_LT(crowd.getPosition(waiting).x, 0.0F);
    EXPECT_TRUE(crowd.completeLink(waiting));
    for (int step = 0; step < 60; ++step) {
        crowd.step(kStep);
    }
    EXPECT_GT(crowd.getPosition(waiting).x, 5.0F);
}

TEST_F(Crowd3DTest, KeepsIdsOfRemovedAgentsFromNamingNewOnes) {
    const NavMesh mesh = plaza();
    Crowd crowd(mesh, {.maxAgents = 2});
    const Crowd::AgentId first = crowd.addAgent({.position = {-10.0F, 0.0F, 0.0F}});
    EXPECT_TRUE(crowd.removeAgent(first));
    const Crowd::AgentId second = crowd.addAgent({.position = {-10.0F, 0.0F, 0.0F}});
    EXPECT_NE(first, second);
    EXPECT_FALSE(crowd.hasAgent(first));
    EXPECT_TRUE(crowd.hasAgent(second));

    crowd.addAgent({.position = {-12.0F, 0.0F, 0.0F}});
    try {
        crowd.addAgent({.position = {-14.0F, 0.0F, 0.0F}});
        FAIL() << "A full crowd took another agent.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The crowd is full with its 2 agents, the \"maxAgents\" it was made with.");
    }
}

} // namespace haylen::navigation3d
