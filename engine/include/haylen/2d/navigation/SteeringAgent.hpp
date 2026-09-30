#pragma once

#include <span>

#include "haylen/math/Circle.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::navigation2d {

// A moving body for steering behaviors. Each behavior returns the velocity change it wants, and `apply` turns the velocity toward the sum of those changes.
struct SteeringAgent {
    math::Vec2 position;
    math::Vec2 velocity;
    float maxSpeed = 120.0F;
    float maxForce = 600.0F;

    [[nodiscard]] math::Vec2 seek(math::Vec2 target) const noexcept;
    [[nodiscard]] math::Vec2 flee(math::Vec2 threat) const noexcept;

    // Seeks the target and slows down linearly inside the slowing radius, stopping on it.
    [[nodiscard]] math::Vec2 arrive(math::Vec2 target, float slowingRadius) const noexcept;

    // Pushes away from every neighbor closer than the radius, harder the closer it is. A neighbor on the exact same spot gives no direction and is ignored.
    [[nodiscard]] math::Vec2 separation(std::span<const math::Vec2> neighbors, float radius) const noexcept;

    // Turns toward the average velocity of the neighbors at full speed, which keeps a flock heading the same way.
    [[nodiscard]] math::Vec2 alignment(std::span<const math::Vec2> neighborVelocities) const noexcept;

    // Seeks the center of the neighbors, which keeps a flock together.
    [[nodiscard]] math::Vec2 cohesion(std::span<const math::Vec2> neighborPositions) const noexcept;

    // Swerves around the closest circle that the path ahead of the agent enters within the look-ahead distance, sideways and harder the closer it is, the obstacle avoidance of Reynolds. Circles should include the radius of the agent.
    [[nodiscard]] math::Vec2 avoid(std::span<const math::Circle> obstacles, float lookAhead) const noexcept;

    // Changes the velocity by the wanted amount at up to `maxForce` per second, caps the speed at `maxSpeed` and moves the agent.
    void apply(math::Vec2 force, float deltaSeconds) noexcept;
};

} // namespace haylen::navigation2d
