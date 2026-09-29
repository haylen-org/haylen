#pragma once

#include <cstddef>
#include <cstdint>
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

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {

// Box2D world in world units. Events collected during step stay available until the next step.
class World final {
  public:
    struct Settings {
        math::Vec2 gravity{0.0F, 980.0F};
        float pixelsPerMeter = 64.0F;
        int subSteps = 4;
    };

    // Throws std::invalid_argument when the scale is not positive or there is no sub-step, and std::runtime_error when Box2D holds too many worlds already.
    explicit World(const Settings& settings = kDefaultSettings);
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Body createBody(const Body::Options& options = {});

    // Anchors are world positions. Distance joints keep the anchors apart, at their current distance unless a length is given. Revolute joints pin the bodies at anchorA, prismatic and wheel joints slide along the axis, and weld joints glue the bodies. Mouse joints pull the second body toward anchorB with 4 hertz, a damping ratio of 1 and enough force to accelerate it at a thousand meters per second squared unless the options set them. Motor joints drive the second body toward its current offset from the first.
    // Prismatic motors, mouse joints and motor joints limit their force with maxMotorForce, and revolute motors, wheel motors and motor joints limit their torque with maxMotorTorque.
    // Revolute and prismatic joints measure their angle from the pose the bodies have at creation, so revolute limits are angles away from that pose. Throws std::invalid_argument for a distance joint shorter than 0.005 meters, revolute limits beyond 0.99 pi radians and any limits whose lower value is above the upper one.
    Joint createJoint(Joint::Type type, Body first, Body second, const Joint::Options& options = {});

    void step(float deltaSeconds);

    // Copies the position and rotation of every body into the values, three floats for each body in order, or moves every body to them, which syncs thousands of sprites with their bodies in one call. Every body must belong to the world, and the values must hold three floats for each one.
    void readTransforms(std::span<const Body> bodies, std::span<float> values) const;
    void writeTransforms(std::span<const Body> bodies, std::span<const float> values);
    void setGravity(math::Vec2 value);
    [[nodiscard]] math::Vec2 getGravity() const;

    [[nodiscard]] const std::vector<ContactEvent>& getContactBegins() const noexcept {
        return contactBegins;
    }
    [[nodiscard]] const std::vector<ContactEvent>& getContactEnds() const noexcept {
        return contactEnds;
    }
    [[nodiscard]] const std::vector<ContactEvent>& getContactHits() const noexcept {
        return contactHits;
    }
    [[nodiscard]] const std::vector<SensorEvent>& getSensorBegins() const noexcept {
        return sensorBegins;
    }
    [[nodiscard]] const std::vector<SensorEvent>& getSensorEnds() const noexcept {
        return sensorEnds;
    }

    // Returns the closest hit of the ray, which Raycaster extends with every other kind of cast.
    [[nodiscard]] std::optional<RaycastHit> raycast(math::Vec2 from, math::Vec2 to, const CollisionFilter& filter = {}) const;

    // Returns the shapes whose bounds overlap the area. Throws std::invalid_argument when its width or height is negative.
    [[nodiscard]] std::vector<Shape> queryRect(const math::Rect& area, const CollisionFilter& filter = {}) const;
    [[nodiscard]] std::vector<Shape> queryCircle(math::Vec2 center, float radius, const CollisionFilter& filter = {}) const;
    [[nodiscard]] std::vector<Shape> queryPoint(math::Vec2 point, const CollisionFilter& filter = {}) const;

    // Draws shape outlines and joints on top of the current canvas, which helps while tuning collisions.
    void debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order = {}) const;

    [[nodiscard]] float getPixelsPerMeter() const noexcept {
        return pixelsPerMeter;
    }
    [[nodiscard]] std::size_t getBodyCount() const;
    [[nodiscard]] std::uint32_t getHandle() const noexcept {
        return handle;
    }

  private:
    friend class Body;
    friend class Shape;
    class OneWayFilter;

    static const Settings kDefaultSettings;
    static debug::ObjectCounter bodyCounter;
    static debug::ObjectCounter contactCounter;

    // One-way platforms keep contacts whose normal leans at least this much toward their direction.
    static constexpr float kOneWayThreshold = 0.5F;

    // Box2D keeps revolute limits within this many radians of zero.
    static const float kRevoluteLimit;

    // Returns the shapes whose bounds overlap the area, in world units.
    [[nodiscard]] std::vector<Shape> overlapBounds(const math::Rect& area, const CollisionFilter& filter) const;

    // Reports the bodies and contacts of the world to the debug statistics.
    void countObjects();

    void checkTransforms(std::span<const Body> bodies, std::size_t valueCount) const;
    static void checkLimits(Joint::Type type, const Joint::Options& options);

    std::uint32_t handle = 0;
    float pixelsPerMeter = 64.0F;
    int subSteps = 4;
    std::vector<ContactEvent> contactBegins;
    std::vector<ContactEvent> contactEnds;
    std::vector<ContactEvent> contactHits;
    std::vector<SensorEvent> sensorBegins;
    std::vector<SensorEvent> sensorEnds;

    // The pre-solve callback of Box2D reads it while the world steps, so it only changes between steps.
    std::unordered_map<std::uint64_t, math::Vec2> oneWayShapes;
    debug::TrackedCount liveBodies{bodyCounter};
    debug::TrackedCount liveContacts{contactCounter};
};

} // namespace haylen::physics2d
