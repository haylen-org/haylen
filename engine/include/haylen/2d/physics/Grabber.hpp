#pragma once

#include <optional>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Drags dynamic bodies with a mouse, a finger or a cursor alike. It takes the body nearest to a point within its pick radius, holds it by the point it took, and pulls that point toward a target with a soft spring whose force grows with everything joined to the body, so one hand of a ragdoll drags the whole figure and a heavy chain follows as well as a pebble. The grabber owns a static anchor body and must go before its world.
class Grabber final {
  public:
    // The pick radius is in world units. The strength is the pull as a multiple of the weight under standard gravity, 9.8 meters per second squared, of the bodies the held one is joined to, so the same strength drags a coin and a ragdoll alike.
    struct Options {
        float pickRadius = 0.0F;
        float strength = 30.0F;
        float hertz = 5.0F;
        float dampingRatio = 0.7F;
        CollisionFilter filter{};
    };

    // Throws `std::invalid_argument` for a negative pick radius, or a strength, stiffness or damping that is not positive.
    Grabber(World& owner, const Options& settings);
    ~Grabber();

    Grabber(const Grabber&) = delete;
    Grabber& operator=(const Grabber&) = delete;

    // Takes the dynamic body nearest to the point, releasing the one it held, and returns it, or nothing when no dynamic body is within the pick radius.
    std::optional<Body> grab(math::Vec2 point);
    void moveTo(math::Vec2 value);
    void release();

    [[nodiscard]] bool isHolding() const;
    [[nodiscard]] std::optional<Body> getBody() const;
    [[nodiscard]] math::Vec2 getTarget() const noexcept {
        return target;
    }
    // Returns where the held point of the body is now, which draws the line of the drag.
    [[nodiscard]] math::Vec2 getHandle() const;
    // Returns the force the pull has, in world units, which `grab` sized for the bodies the held one is joined to.
    [[nodiscard]] float getForce() const;

    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    void setPickRadius(float value);
    void setStrength(float value);
    void setHertz(float value);
    void setDampingRatio(float value);

  private:
    // Standard gravity in meters per second squared, which the strength measures the pull against.
    static constexpr float kStandardGravity = 9.80665F;

    // Returns the mass of the body and of every dynamic body joined to it, directly or through others.
    [[nodiscard]] float getConnectedMass(const Body& body) const;

    World& world;
    Options options;
    Body anchor;
    Body held;
    Joint joint;
    math::Vec2 target{};
    // The held point in the space of the body, in meters.
    math::Vec2 handle{};
};

} // namespace haylen::physics2d
