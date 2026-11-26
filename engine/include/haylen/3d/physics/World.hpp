#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "haylen/3d/physics/Backend.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d {

// A 3D physics world in meters, which the backend it was made with simulates. The world checks every value it receives, so each backend keeps the same contract, and reports once each limit that a step runs into.
class World final {
  public:
    // Throws `std::invalid_argument` for settings out of their range and for more than one thread without a job system, and passes on the `std::runtime_error` of a backend that the device cannot run.
    World(const WorldSettings& initial, Backend& backend, core::JobSystem* jobSystem = nullptr);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Ends the simulation at once with everything it holds, before the last owner lets the object go. Every later call that needs the simulation throws `std::logic_error`. Destroying a destroyed world does nothing.
    void destroy() noexcept;
    [[nodiscard]] bool isDestroyed() const noexcept {
        return simulation == nullptr;
    }

    // Advances the world by a time of zero seconds or more. Throws `std::invalid_argument` for a negative time or one that is not a number.
    void step(float deltaSeconds);

    // The settings that change while the world lives. Each setter throws `std::invalid_argument` for a value out of its range, as `WorldSettings::validate` checks it.
    void setGravity(math::Vec3 value);
    [[nodiscard]] math::Vec3 getGravity() const noexcept {
        return settings.gravity;
    }
    void setCollisionSteps(int value);
    [[nodiscard]] int getCollisionSteps() const noexcept {
        return settings.collisionSteps;
    }
    void setVelocitySteps(int value);
    [[nodiscard]] int getVelocitySteps() const noexcept {
        return settings.velocitySteps;
    }
    void setPositionSteps(int value);
    [[nodiscard]] int getPositionSteps() const noexcept {
        return settings.positionSteps;
    }
    void setSleepEnabled(bool value);
    [[nodiscard]] bool isSleepEnabled() const noexcept {
        return settings.sleepEnabled;
    }
    void setTimeBeforeSleep(float value);
    [[nodiscard]] float getTimeBeforeSleep() const noexcept {
        return settings.timeBeforeSleep;
    }
    void setSleepVelocity(float value);
    [[nodiscard]] float getSleepVelocity() const noexcept {
        return settings.sleepVelocity;
    }
    void setSpeculativeContactDistance(float value);
    [[nodiscard]] float getSpeculativeContactDistance() const noexcept {
        return settings.speculativeContactDistance;
    }
    void setPenetrationSlop(float value);
    [[nodiscard]] float getPenetrationSlop() const noexcept {
        return settings.penetrationSlop;
    }
    void setRestitutionThreshold(float value);
    [[nodiscard]] float getRestitutionThreshold() const noexcept {
        return settings.restitutionThreshold;
    }

    // The settings of the world as they are now, with the limits it was made with.
    [[nodiscard]] const WorldSettings& getSettings() const noexcept {
        return settings;
    }
    [[nodiscard]] std::string_view getBackendName() const noexcept {
        return backendName;
    }
    [[nodiscard]] float getLastStep() const noexcept {
        return lastStep;
    }

    // Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] WorldStats getStats() const;

    // Hashes the whole state that the steps advance bit for bit, so two runs that should match compare one number. Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] std::uint64_t computeStateHash() const;

  private:
    // Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] Simulation& getSimulation() const;

    // Checks the settings with a changed value and hands them to the simulation, so a value out of range leaves the world as it was.
    void reconfigure(const WorldSettings& changed);
    void reportLimits(const Simulation::StepReport& report);

    WorldSettings settings;
    std::string backendName;
    std::unique_ptr<Simulation> simulation;
    Simulation::StepReport reported;
    float lastStep = 0.0F;
};

} // namespace haylen::physics3d
