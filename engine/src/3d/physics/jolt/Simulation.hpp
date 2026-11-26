#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>

#include "3d/physics/jolt/Layers.hpp"
#include "3d/physics/jolt/StepAllocator.hpp"
#include "3d/physics/jolt/StepJobs.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d::jolt {

// The simulation of a world by a physics system of Jolt.
class Simulation final : public physics3d::Simulation {
  public:
    // A world steps on its threads only while this many bodies are awake, because below it waking the workers costs more than they save.
    static constexpr JPH::uint kParallelBodies = 1000;

    Simulation(const WorldSettings& settings, core::JobSystem* jobSystem);
    ~Simulation() override;

    [[nodiscard]] StepReport step(float deltaSeconds) override;
    void configure(const WorldSettings& settings) override;
    [[nodiscard]] WorldStats getStats() const override;
    [[nodiscard]] std::uint64_t computeStateHash() const override;

    // The physics system, which the bodies, shapes and constraints of the world live in.
    [[nodiscard]] JPH::PhysicsSystem& getSystem() noexcept {
        return system;
    }

  private:
    static constexpr std::size_t kMegabyte = std::size_t{1} << 20;

    Layers layers;
    JPH::PhysicsSystem system;
    StepAllocator allocator;
    std::shared_ptr<StepJobs> jobs;
    int collisionSteps = 1;
    int stepThreads = 0;
    float stepMilliseconds = 0.0F;
};

} // namespace haylen::physics3d::jolt
