#include <gtest/gtest.h>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <memory>
#include <string>
#include <vector>

#include "3d/physics/jolt/Layers.hpp"
#include "3d/physics/jolt/Library.hpp"
#include "3d/physics/jolt/Simulation.hpp"
#include "3d/physics/jolt/StepAllocator.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/core/JobSystem.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::physics3d::jolt {

// The bodies of these tests come straight from Jolt, since they check the parts of the backend under the bodies of the engine.
class SimulationTest : public ::testing::Test {
  protected:
    static constexpr float kStep = 1.0F / 60.0F;
    static constexpr float kHalfBox = 0.5F;

    void SetUp() override {
        Library::initialize();
    }

    // Adds a ground whose top lies at zero, a hundred meters wide.
    static void addGround(Simulation& simulation) {
        JPH::BoxShapeSettings shape(JPH::Vec3(50.0F, 0.5F, 50.0F));
        shape.SetEmbedded();
        const JPH::BodyCreationSettings ground(shape.Create().Get(), JPH::RVec3(0.0F, -0.5F, 0.0F), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::getStaticLayer());
        simulation.getSystem().GetBodyInterface().CreateAndAddBody(ground, JPH::EActivation::DontActivate);
    }

    // Adds a box of one meter whose center lies at the position.
    static JPH::BodyID addBox(Simulation& simulation, JPH::RVec3 position) {
        JPH::BoxShapeSettings shape(JPH::Vec3(kHalfBox, kHalfBox, kHalfBox));
        shape.SetEmbedded();
        const JPH::BodyCreationSettings box(shape.Create().Get(), position, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::getMovingLayer());
        return simulation.getSystem().GetBodyInterface().CreateAndAddBody(box, JPH::EActivation::Activate);
    }

    // Drops a grid of boxes onto the ground, more than a world steps on its threads for.
    static std::vector<JPH::BodyID> addPile(Simulation& simulation) {
        std::vector<JPH::BodyID> boxes;
        for (int layer = 0; layer < 4; ++layer) {
            for (int row = 0; row < 18; ++row) {
                for (int column = 0; column < 18; ++column) {
                    const JPH::RVec3 position(static_cast<float>(column) * 1.5F - 13.0F, 1.0F + static_cast<float>(layer) * 1.5F, static_cast<float>(row) * 1.5F - 13.0F);
                    boxes.push_back(addBox(simulation, position));
                }
            }
        }

        return boxes;
    }

    core::JobSystem jobs{[](const std::string& error) { FAIL() << error; }};
};

TEST_F(SimulationTest, LetsABoxRestStillOnAPlane) {
    Simulation simulation({}, nullptr);
    addGround(simulation);
    const JPH::BodyID box = addBox(simulation, JPH::RVec3(0.0F, 2.0F, 0.0F));
    for (int step = 0; step < 180; ++step) {
        const Simulation::StepReport report = simulation.step(kStep);
        ASSERT_FALSE(report.bodyPairsFull || report.contactConstraintsFull || report.temporaryMemoryFull);
    }

    const JPH::BodyInterface& bodies = simulation.getSystem().GetBodyInterface();
    EXPECT_NEAR(bodies.GetPosition(box).GetY(), kHalfBox, 0.02F);
    EXPECT_LT(bodies.GetLinearVelocity(box).Length(), 0.01F);
    EXPECT_FALSE(bodies.IsActive(box));
    EXPECT_EQ(simulation.getStats().bodies, 2);
    EXPECT_EQ(simulation.getStats().activeBodies, 0);
}

TEST_F(SimulationTest, StepsAPileOnTheWorkersOfTheEngineTheSameWayEveryRun) {
    if (jobs.getWorkerCount() == 0) {
        GTEST_SKIP() << "This platform runs no worker besides the thread of the frames.";
    }

    const WorldSettings settings{.threads = 4};
    std::vector<std::uint64_t> hashes;
    for (int run = 0; run < 2; ++run) {
        Simulation simulation(settings, &jobs);
        addGround(simulation);
        const std::vector<JPH::BodyID> boxes = addPile(simulation);
        static_cast<void>(simulation.step(kStep));
        EXPECT_EQ(simulation.getStats().stepThreads, 4);
        for (int step = 0; step < 90; ++step) {
            static_cast<void>(simulation.step(kStep));
        }

        const JPH::BodyInterface& bodies = simulation.getSystem().GetBodyInterface();
        for (const JPH::BodyID& box : boxes) {
            ASSERT_GT(bodies.GetPosition(box).GetY(), kHalfBox - 0.05F);
        }

        hashes.push_back(simulation.computeStateHash());
    }

    EXPECT_EQ(hashes[0], hashes[1]);
}

TEST_F(SimulationTest, StepsABoxThatStaysAwakeWithoutAllocating) {
    Simulation simulation(WorldSettings{.sleepEnabled = false}, nullptr);
    addGround(simulation);
    addBox(simulation, JPH::RVec3(0.0F, 1.0F, 0.0F));
    for (int step = 0; step < 60; ++step) {
        static_cast<void>(simulation.step(kStep));
    }

    const test::AllocationTracker tracker;
    for (int step = 0; step < 10; ++step) {
        static_cast<void>(simulation.step(kStep));
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(simulation.getStats().activeBodies, 1);
}

TEST_F(SimulationTest, AllocatesWhatAStepNeedsBeyondItsTemporaryMemoryAndRecordsIt) {
    StepAllocator allocator(256);
    void* inside = allocator.Allocate(128);
    EXPECT_FALSE(allocator.takeOverflow());
    void* beyond = allocator.Allocate(1024);
    EXPECT_TRUE(allocator.takeOverflow());
    EXPECT_FALSE(allocator.takeOverflow());

    allocator.Free(beyond, 1024);
    allocator.Free(inside, 128);
    EXPECT_EQ(allocator.getPeak(), 128U);
}

} // namespace haylen::physics3d::jolt
