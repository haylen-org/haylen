#pragma once

#include <cstdint>

#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

// The simulation of one 3D physics world, which a backend makes for `physics3d::World`. The world checks every value before it reaches the simulation and calls it on the thread that steps the world alone, so a simulation trusts its input and needs no lock of its own.
class Simulation {
  public:
    // The limits that a step ran into, whose contacts or temporary data the step left out, which the world reports once each.
    struct StepReport {
        bool bodyPairsFull = false;
        bool contactConstraintsFull = false;
        bool temporaryMemoryFull = false;
    };

    Simulation() = default;
    virtual ~Simulation() = default;

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    // Advances the world by the time in the collision steps of the settings.
    [[nodiscard]] virtual StepReport step(float deltaSeconds) = 0;

    // Takes the settings that may change while the world lives: the gravity, the steps, the sleep and the tuning of contacts. The limits keep the values the simulation was made with.
    virtual void configure(const WorldSettings& settings) = 0;

    [[nodiscard]] virtual WorldStats getStats() const = 0;

    // Hashes the whole state that the simulation advances bit for bit, so two runs that should match compare one number.
    [[nodiscard]] virtual std::uint64_t computeStateHash() const = 0;
};

} // namespace haylen::physics3d
