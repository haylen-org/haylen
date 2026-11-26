#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/3d/physics/Backend.hpp"
#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d {

// A 3D physics world in meters, which the backend it was made with simulates. The world checks every value it receives, so each backend keeps the same contract, and reports once each limit that a step runs into.
class World final {
  public:
    // Hooks run before the backend advances, where forces apply, or after it, where the new positions are known.
    enum class StepPhase : std::uint8_t {
        Before,
        After,
    };

    using StepHook = std::function<void(float deltaSeconds)>;

    // The floats that `readTransforms` and `writeTransforms` hold for each body: the position and the rotation.
    static constexpr std::size_t kTransformFloats = 7;

    // Throws `std::invalid_argument` for settings out of their range and for more than one thread without a job system, `std::runtime_error` when the process holds as many live worlds as `WorldSlots` keeps, and passes on the `std::runtime_error` of a backend that the device cannot run.
    World(const WorldSettings& initial, Backend& backend, core::JobSystem* jobSystem = nullptr);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Ends the simulation at once with every body and hook it holds, before the last owner lets the object go. Every handle of the world turns invalid, and every later call that needs the simulation throws `std::logic_error`. Destroying a destroyed world does nothing.
    void destroy() noexcept;
    [[nodiscard]] bool isDestroyed() const noexcept {
        return simulation == nullptr;
    }

    // Makes a body with a shape of the backend of the world. A dynamic body needs a solid shape, a plane suits static and kinematic bodies alone, and a dynamic body keeps at least one axis free. Throws `std::invalid_argument` with a sentence that names the option for a value that is not finite or out of its range, and `std::length_error` when the world holds `maxBodies` already.
    Body createBody(const Body::Options& options);

    // Runs the hooks before the step, advances the world, records the transforms of the bodies that moved when the world interpolates and runs the hooks after the step. Throws `std::invalid_argument` for a negative time or one that is not a number.
    void step(float deltaSeconds);

    // Adds a function that runs at the given phase of every step until it is removed or the world is destroyed, which is how helpers act on the world in C++, and returns its id. A hook may add and remove hooks, and may destroy the world, which ends the step: each phase runs the hooks it had when it began, except those removed since.
    std::uint64_t addStepHook(StepPhase phase, StepHook hook);
    void removeStepHook(std::uint64_t id);

    // Copies the transform of every body into the values, seven floats for each body in order, `x`, `y`, `z` and the `x`, `y`, `z` and `w` of its rotation, or moves every body to them, which syncs thousands of models with their bodies in one call. With a blend from 0 to 1, a world that interpolates gives each body between its transforms of the last two steps. Every body must be a live body of the world, and the values must hold seven floats for each one. Throws `std::invalid_argument` otherwise, and `std::logic_error` for a blend in a world that does not interpolate.
    void readTransforms(std::span<const Body> bodies, std::span<float> values, std::optional<float> blend = std::nullopt) const;
    void writeTransforms(std::span<const Body> bodies, std::span<const float> values);

    // Returns the transform of a body between the last two steps, which draws it smoothly when frames and fixed steps do not line up. Bodies that the world did not see move, including those a teleport placed, keep their current transform. Throws `std::logic_error` unless the world interpolates.
    [[nodiscard]] Body::Transform getInterpolatedTransform(const Body& body, float blend) const;

    // Moves the ids of the bodies destroyed since the last call into `ids`, which lets owners of per-body data release it without scanning every body.
    void takeDestroyedBodies(std::vector<std::uint32_t>& ids);

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

    // The removal of internal edges that new bodies take unless they set their own.
    void setEnhancedInternalEdgeRemoval(bool value) noexcept {
        settings.enhancedInternalEdgeRemoval = value;
    }
    [[nodiscard]] bool hasEnhancedInternalEdgeRemoval() const noexcept {
        return settings.enhancedInternalEdgeRemoval;
    }
    [[nodiscard]] bool isInterpolating() const noexcept {
        return settings.interpolate;
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

    // The key of the slot and generation of the world, which its handles keep, or 0 once the world is destroyed.
    [[nodiscard]] std::uint64_t getKey() const noexcept {
        return key;
    }

    // Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] WorldStats getStats() const;

    // Hashes the whole state that the steps advance bit for bit, so two runs that should match compare one number. Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] std::uint64_t computeStateHash() const;

  private:
    friend class Body;

    struct Hook {
        std::uint64_t id = 0;
        StepPhase phase = StepPhase::Before;
        StepHook function;
        bool removed = false;
    };

    // Drops the hooks a phase took into the shared list of running hooks when the phase ends, even when a hook throws.
    class RunningScope final {
      public:
        RunningScope(std::vector<std::shared_ptr<Hook>>& list, std::size_t size) noexcept : entries(list), base(size) {}
        ~RunningScope() {
            entries.resize(base);
        }

        RunningScope(const RunningScope&) = delete;
        RunningScope& operator=(const RunningScope&) = delete;

      private:
        std::vector<std::shared_ptr<Hook>>& entries;
        std::size_t base;
    };

    // The transforms a body had after the last two steps it moved in, at the slot of the body.
    struct Motion {
        Simulation::BodyId id = 0;
        std::uint64_t step = 0;
        Body::Transform previous;
        Body::Transform current;
    };

    static debug::ObjectCounter& bodyCounter;

    // Throws `std::logic_error` once the world is destroyed.
    [[nodiscard]] Simulation& getSimulation() const;
    void checkLive() const;

    // Checks the settings with a changed value and hands them to the simulation, so a value out of range leaves the world as it was.
    void reconfigure(const WorldSettings& changed);
    void reportLimits(const Simulation::StepReport& report);
    void runHooks(StepPhase phase, float deltaSeconds);
    void recordMotion();
    [[nodiscard]] Body::Transform blendMotion(Simulation::BodyId id, float blend) const;
    void checkTransforms(std::span<const Body> bodies, std::size_t valueCount) const;

    // Fills `transformIds` with the ids of the bodies, the storage that every read and write of transforms reuses.
    void collectIds(std::span<const Body> bodies) const;

    // The body leaves the simulation, its motion and the count of bodies, and the world lists it as destroyed. Called by `Body` once the body is gone.
    void forgetBody(Simulation::BodyId id);

    // A teleport puts a body where the world did not see it move, so it draws there at once.
    void forgetMotion(Simulation::BodyId id);

    WorldSettings settings;
    std::string backendName;
    std::unique_ptr<Simulation> simulation;
    std::uint64_t key = 0;
    Simulation::StepReport reported;
    float lastStep = 0.0F;
    std::size_t bodyCount = 0;

    std::vector<std::shared_ptr<Hook>> hooks;
    std::vector<std::shared_ptr<Hook>> runningHooks;
    std::uint64_t nextHook = 1;

    std::vector<Motion> motions;
    std::vector<Simulation::BodyId> activeBodies;
    std::uint64_t motionStep = 0;
    std::vector<std::uint32_t> destroyedBodies;
    mutable std::vector<Simulation::BodyId> transformIds;
};

} // namespace haylen::physics3d
