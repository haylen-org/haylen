#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A kinematic character: an upright capsule that moves by the displacements an app asks for and slides along what it hits, without the solver pushing it around. It stands still on slopes up to its slope limit, keeps its speed along them, steps onto ledges up to its step height, stays on the ground over crests and down slopes, rides the bodies it stands on, stops at ceilings and walls, passes one-way platforms from their closed side and pushes dynamic bodies. The capsule that collides covers the mover above its step height, and a ray under it finds the ground, so steps rise under it smoothly. A kinematic body of that capsule follows the mover, so other bodies collide with it too. The mover owns that body and must go before its world.
class Mover final {
  public:
    // The position is the center of the mover and the height runs from its feet to the top of its head. The slope limit is the steepest ground in radians it stands on and walks up. Snapping keeps it on the ground within `snapDistance` below its feet while it does not move up. The push force, in world units, is the most force it pushes the dynamic bodies it walks into with.
    struct Options {
        math::Vec2 position{};
        float radius = 12.0F;
        float height = 48.0F;
        float maxSlope = 0.87F;
        float stepHeight = 0.0F;
        float snapDistance = 8.0F;
        float pushForce = 20000.0F;
        CollisionFilter filter{};
    };

    // Throws `std::invalid_argument` for a radius of 0.01 meters or less, a height that leaves less than twice the radius above the step height, a slope limit that is negative or not below half pi radians, or a negative step height, snap distance or push force.
    Mover(World& owner, const Options& settings);
    ~Mover();

    Mover(const Mover&) = delete;
    Mover& operator=(const Mover&) = delete;

    // Moves by `delta` in world units, sliding along what it hits, and returns how far it went. On walkable ground a downward part of the delta, such as gravity, holds it on the ground instead of sliding it down, and the sideways part follows the ground at its full length. The ground it stands on carries it by how far it moved since the last move.
    math::Vec2 move(math::Vec2 delta);

    // Removes from a velocity what pushes into the ground, walls and ceilings of the last move, which stops a fall on landing and a jump at a ceiling. The speed toward dynamic bodies stays, because the mover pushes them.
    [[nodiscard]] math::Vec2 clip(math::Vec2 velocity) const;

    // Lets the mover pass through every one-way platform for `seconds`, and through a platform it is still inside after that until it leaves it. Throws `std::invalid_argument` for a negative time.
    void dropThrough(float seconds);

    // Teleports the mover and its body.
    void setPosition(math::Vec2 value);
    [[nodiscard]] math::Vec2 getPosition() const noexcept {
        return position;
    }

    [[nodiscard]] bool isGrounded() const noexcept {
        return grounded;
    }
    [[nodiscard]] math::Vec2 getGroundNormal() const noexcept {
        return groundNormal;
    }
    [[nodiscard]] std::optional<Body> getGroundBody() const;
    // The velocity of the ground under the mover, which a jump from a moving platform keeps.
    [[nodiscard]] math::Vec2 getGroundVelocity() const;
    [[nodiscard]] bool isOnWall() const noexcept {
        return onWall;
    }
    [[nodiscard]] bool isOnCeiling() const noexcept {
        return onCeiling;
    }

    [[nodiscard]] Body getBody() const noexcept {
        return body;
    }
    [[nodiscard]] bool isValid() const noexcept {
        return body.isValid();
    }
    // Destroys the body that follows the mover, after which the mover no longer moves.
    void destroy();

    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    void setMaxSlope(float value);
    void setSnapDistance(float value);

  private:
    class Query;

    // A surface the capsule touches: its normal toward the mover and how deep the mover is in it, in meters.
    struct Plane {
        math::Vec2 normal{};
        float offset = 0.0F;
        std::uint64_t shape = 0;
    };

    // What a sweep of the capsule or the ground ray hit: how far along it went, the normal there, the point in meters and the shape.
    struct Probe {
        bool hit = false;
        float fraction = 1.0F;
        math::Vec2 normal{};
        math::Vec2 point{};
        std::uint64_t shape = 0;
    };

    static constexpr std::size_t kPlaneCapacity = 16;
    static constexpr int kIterations = 5;

    // Moves shorter than this many meters end the sliding, and the mover stands this many meters above the ground it finds.
    static constexpr float kTolerance = 0.01F;
    static constexpr float kSkin = 0.005F;

    [[nodiscard]] bool isWalkable(math::Vec2 normal) const noexcept;
    void gatherPlanes(bool flattenSteep);
    // Slides through the planes toward the delta and returns how far it went.
    math::Vec2 slide(math::Vec2 delta, bool flattenSteep);
    [[nodiscard]] Probe cast(math::Vec2 from, math::Vec2 translation) const;
    // Casts the ground ray from inside the bottom of the capsule down to `reach` world units below the feet.
    [[nodiscard]] Probe castGround(float reach) const;
    // Finds the ground under the mover and stands on it when it lies within the step height above the feet or, when snapping, the snap distance below them.
    void stand(bool snap);
    [[nodiscard]] math::Vec2 followGround() const;
    void push(math::Vec2 delta);
    void moveBody();

    World& world;
    Options options;
    Body body;
    math::Vec2 position{};
    float scale = 64.0F;
    std::array<Plane, kPlaneCapacity> planes{};
    std::size_t planeCount = 0;
    bool grounded = false;
    bool onWall = false;
    bool onCeiling = false;
    math::Vec2 groundNormal{0.0F, -1.0F};
    std::uint64_t groundShape = 0;
    // The point under the feet in meters, in the world and in the space of the ground body.
    math::Vec2 groundPoint{};
    math::Vec2 groundAnchor{};
};

} // namespace haylen::physics2d
