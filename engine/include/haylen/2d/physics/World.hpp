#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/ContactEvent.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/2d/physics/RaycastHit.hpp"
#include "haylen/2d/physics/SensorEvent.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedCount.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {

class StepTasks;

// Box2D world in world units. Events collected during `step` stay available until the next step.
class World final {
  public:
    // Speeds are in world units per second. Unset speeds keep the values Box2D is tuned for in meters: a speed limit of 400 meters per second, overlaps pushed apart at up to 3 meters per second, and bounces and hit events from 1 meter per second. More than one thread steps the world on the job system the world receives, except on the web, which runs on one thread.
    struct Settings {
        math::Vec2 gravity{0.0F, 980.0F};
        float pixelsPerMeter = 64.0F;
        int subSteps = 4;
        int threads = 1;
        bool continuous = true;
        bool sleepEnabled = true;
        bool interpolate = false;
        float contactHertz = 30.0F;
        float contactDampingRatio = 10.0F;
        std::optional<float> contactPushSpeed;
        std::optional<float> maxSpeed;
        std::optional<float> restitutionThreshold;
        std::optional<float> hitThreshold;
    };

    // A world of more than one thread steps on its threads only while this many bodies are awake, because below it waking the workers costs more than they save.
    static constexpr int kParallelBodies = 1000;

    // One-way platforms keep contacts whose normal leans at least this much toward their direction. A body starts standing on one only when it is at most `kOneWayDepth` meters inside it, so a body that rises halfway through falls back instead of popping up.
    static constexpr float kOneWayThreshold = 0.5F;
    static constexpr float kOneWayDepth = 0.02F;

    // The counts of the world and the time each phase of the last step took.
    struct Stats {
        int bodies = 0;
        int awakeBodies = 0;
        int shapes = 0;
        int contacts = 0;
        int joints = 0;
        int islands = 0;
        float stepMilliseconds = 0.0F;
        float collideMilliseconds = 0.0F;
        float solveMilliseconds = 0.0F;
        float continuousMilliseconds = 0.0F;
        float sleepMilliseconds = 0.0F;
        float hookMilliseconds = 0.0F;
    };

    // A joint the last step destroyed because its force or torque passed its break limit, with the force and torque it held.
    struct JointBreak {
        Joint joint;
        Body first;
        Body second;
        math::Vec2 force{};
        float torque = 0.0F;
    };

    struct Transform {
        math::Vec2 position{};
        float rotation = 0.0F;
    };

    // Hooks run before Box2D advances, where forces apply, or after it, where the new positions are known.
    enum class StepPhase : std::uint8_t {
        Before,
        After,
    };

    using StepHook = std::function<void(float deltaSeconds)>;

    // Throws `std::invalid_argument` when the scale is not positive, there is no sub-step or thread, or more than one thread has no job system, and `std::runtime_error` when Box2D holds too many worlds already.
    explicit World(const Settings& settings = kDefaultSettings, core::JobSystem* jobSystem = nullptr);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Body createBody(const Body::Options& options = {});

    // Anchors are world positions. Distance joints keep the anchors apart, at their current distance unless a length is given. Revolute joints pin the bodies at `anchorA`, prismatic and wheel joints slide along the axis, and weld joints glue the bodies. Mouse joints pull the second body toward `anchorB` with 4 hertz, a damping ratio of 1 and enough force to accelerate it at a thousand meters per second squared unless the options set them. Motor joints drive the second body toward its current offset from the first.
    // Prismatic motors, distance motors, mouse joints and motor joints limit their force with `maxMotorForce`, and revolute motors, wheel motors and motor joints limit their torque with `maxMotorTorque`. A joint with a break force or torque is destroyed by the first step that loads it beyond them.
    // Revolute and prismatic joints measure their angle from the pose the bodies have at creation, so revolute limits are angles away from that pose. Throws `std::invalid_argument` for a distance joint shorter than 0.005 meters, revolute limits beyond 0.99 pi radians, any limits whose lower value is above the upper one and negative break limits.
    Joint createJoint(Joint::Type type, Body first, Body second, const Joint::Options& options = {});

    // Runs the hooks before the step, advances the world, breaks the joints loaded beyond their limits and runs the hooks after the step.
    void step(float deltaSeconds);

    // Adds a function that runs at the given phase of every step until it is removed, which is how vehicles, force fields and fluids act on the world, and returns its id. Hooks never add or remove hooks while they run.
    std::uint64_t addStepHook(StepPhase phase, StepHook hook);
    void removeStepHook(std::uint64_t id);

    // Copies the position and rotation of every body into the values, three floats for each body in order, or moves every body to them, which syncs thousands of sprites with their bodies in one call. With a blend from 0 to 1, a world that interpolates gives each body between its transforms of the last two steps. Every body must belong to the world, and the values must hold three floats for each one.
    void readTransforms(std::span<const Body> bodies, std::span<float> values, std::optional<float> blend = std::nullopt) const;
    void writeTransforms(std::span<const Body> bodies, std::span<const float> values);

    // Returns the transform of a body between the last two steps, which draws it smoothly when frames and fixed steps do not line up. Bodies the world did not see move, including those a teleport placed, keep their current transform. Throws `std::logic_error` unless the world interpolates.
    [[nodiscard]] Transform getInterpolatedTransform(const Body& body, float blend) const;

    // Changing gravity wakes every body, so sleeping piles react.
    void setGravity(math::Vec2 value);
    [[nodiscard]] math::Vec2 getGravity() const;
    void wakeAll();

    // Throws `std::invalid_argument` for fewer than one sub-step.
    void setSubSteps(int value);
    [[nodiscard]] int getSubSteps() const noexcept {
        return subSteps;
    }
    void setContinuousEnabled(bool value);
    [[nodiscard]] bool isContinuousEnabled() const;
    void setSleepEnabled(bool value);
    [[nodiscard]] bool isSleepEnabled() const;
    void setMaxSpeed(float value);
    [[nodiscard]] float getMaxSpeed() const;
    void setContactHertz(float value);
    [[nodiscard]] float getContactHertz() const noexcept {
        return contactHertz;
    }
    void setContactDampingRatio(float value);
    [[nodiscard]] float getContactDampingRatio() const noexcept {
        return contactDampingRatio;
    }
    void setContactPushSpeed(float value);
    [[nodiscard]] float getContactPushSpeed() const noexcept {
        return contactPushSpeed;
    }
    void setRestitutionThreshold(float value);
    [[nodiscard]] float getRestitutionThreshold() const;
    void setHitThreshold(float value);
    [[nodiscard]] float getHitThreshold() const;
    [[nodiscard]] int getThreads() const noexcept {
        return threads;
    }
    [[nodiscard]] bool isInterpolating() const noexcept {
        return interpolate;
    }
    // Tells whether a body passes through one-way platforms now because of `Body::dropThrough`.
    [[nodiscard]] bool isDroppingThrough(const Body& body) const;

    [[nodiscard]] float getLastStep() const noexcept {
        return lastStep;
    }

    [[nodiscard]] const std::vector<ContactEvent>& getContactBegins() const;
    [[nodiscard]] const std::vector<ContactEvent>& getContactEnds() const;
    [[nodiscard]] const std::vector<ContactEvent>& getContactHits() const;
    [[nodiscard]] const std::vector<SensorEvent>& getSensorBegins() const;
    [[nodiscard]] const std::vector<SensorEvent>& getSensorEnds() const;
    [[nodiscard]] const std::vector<JointBreak>& getJointBreaks() const noexcept {
        return jointBreaks;
    }

    // Moves the ids of the bodies destroyed since the last call into `ids`, which lets owners of per-body data release it without scanning every body.
    void takeDestroyedBodies(std::vector<std::uint64_t>& ids);

    // Returns the closest hit of the ray, which `Raycaster` extends with every other kind of cast.
    [[nodiscard]] std::optional<RaycastHit> raycast(math::Vec2 from, math::Vec2 to, const CollisionFilter& filter = {}) const;

    // Returns the shapes whose bounds overlap the area. Throws `std::invalid_argument` when its width or height is negative.
    [[nodiscard]] std::vector<Shape> queryRect(const math::Rect& area, const CollisionFilter& filter = {}) const;
    [[nodiscard]] std::vector<Shape> queryCircle(math::Vec2 center, float radius, const CollisionFilter& filter = {}) const;
    [[nodiscard]] std::vector<Shape> queryPoint(math::Vec2 point, const CollisionFilter& filter = {}) const;

    // Returns the shapes within `radius` of the point, nearest first, where shapes that contain the point come first, which finds thin bodies under a finger.
    [[nodiscard]] std::vector<Shape> pick(math::Vec2 point, float radius, const CollisionFilter& filter = {}) const;

    // Hashes the transforms and velocities of the bodies bit for bit, so two runs that should match can compare one number.
    [[nodiscard]] std::uint64_t computeStateHash(std::span<const Body> bodies) const;

    // Draws shape outlines and joints on top of the current canvas, which helps while tuning collisions.
    void debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order = {}) const;

    [[nodiscard]] float getPixelsPerMeter() const noexcept {
        return pixelsPerMeter;
    }
    [[nodiscard]] std::size_t getBodyCount() const;
    [[nodiscard]] std::size_t getAwakeBodyCount() const;
    [[nodiscard]] Stats getStats() const;
    [[nodiscard]] core::JobSystem* getJobs() const noexcept {
        return jobs;
    }
    [[nodiscard]] std::uint32_t getHandle() const noexcept {
        return handle;
    }

  private:
    friend class Body;
    friend class Shape;
    friend class Joint;
    class OneWayFilter;

    struct Hook {
        std::uint64_t id = 0;
        StepPhase phase = StepPhase::Before;
        StepHook function;
    };

    struct OneWayPlatform {
        math::Vec2 direction{};
        math::Vec2 worldDirection{};
    };

    // The break limits of a joint in newtons and newton meters, the units Box2D reports its loads in.
    struct BreakLimit {
        std::optional<float> force;
        std::optional<float> torque;
    };

    // The transforms a body had after the last two steps it moved in, indexed by the slot of the body.
    struct Motion {
        std::uint64_t id = 0;
        std::uint64_t step = 0;
        Transform previous;
        Transform current;
    };

    // The events of the last step, converted from Box2D only when something reads them.
    struct Events {
        std::vector<ContactEvent> contactBegins;
        std::vector<ContactEvent> contactEnds;
        std::vector<ContactEvent> contactHits;
        std::vector<SensorEvent> sensorBegins;
        std::vector<SensorEvent> sensorEnds;
        bool contactsRead = false;
        bool sensorsRead = false;
    };

    static const Settings kDefaultSettings;
    static debug::ObjectCounter& bodyCounter;
    static debug::ObjectCounter& contactCounter;

    // Box2D steps a world on at most this many workers.
    static constexpr int kMaxThreads = 64;

    // Box2D keeps revolute limits within this many radians of zero.
    static const float kRevoluteLimit;

    // Returns the shapes whose bounds overlap the area, in world units.
    [[nodiscard]] std::vector<Shape> overlapBounds(const math::Rect& area, const CollisionFilter& filter) const;

    // Reports the bodies and contacts of the world to the debug statistics.
    void countObjects();

    void runHooks(StepPhase phase, float deltaSeconds);
    void prepareOneWayPlatforms(float deltaSeconds);
    void breakJoints();
    void recordMotion();
    void readContactEvents() const;
    void readSensorEvents() const;
    void checkTransforms(std::span<const Body> bodies, std::size_t valueCount) const;
    void applyContactTuning();
    void forgetBody(std::uint64_t id);
    [[nodiscard]] Transform blendMotion(const Body& body, float blend) const;
    static void checkLimits(Joint::Type type, const Joint::Options& options);

    std::uint32_t handle = 0;
    float pixelsPerMeter = 64.0F;
    int subSteps = 4;
    int threads = 1;
    bool interpolate = false;
    float contactHertz = 30.0F;
    float contactDampingRatio = 10.0F;
    float contactPushSpeed = 0.0F;
    float lastStep = 0.0F;
    float hookMilliseconds = 0.0F;
    core::JobSystem* jobs = nullptr;
    std::unique_ptr<StepTasks> tasks;
    mutable Events events;
    std::vector<JointBreak> jointBreaks;
    std::vector<std::uint64_t> destroyedBodies;
    std::vector<Hook> hooks;
    std::uint64_t nextHook = 1;
    std::map<std::uint64_t, BreakLimit> breakLimits;
    std::vector<Motion> motions;
    std::uint64_t motionStep = 0;

    // The pre-solve callback of Box2D reads these while the world steps, maybe on several threads, so they only change between steps.
    std::unordered_map<std::uint64_t, OneWayPlatform> oneWayShapes;
    std::unordered_map<std::uint64_t, float> droppingBodies;

    debug::TrackedCount liveBodies{bodyCounter};
    debug::TrackedCount liveContacts{contactCounter};
};

} // namespace haylen::physics2d
