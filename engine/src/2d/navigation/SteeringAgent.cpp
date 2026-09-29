#include "haylen/2d/navigation/SteeringAgent.hpp"

#include <algorithm>
#include <optional>

#include "haylen/math/Ray.hpp"
#include "haylen/math/RayHit.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::navigation2d {

math::Vec2 SteeringAgent::seek(math::Vec2 target) const noexcept {
    return (target - position).getNormalized() * maxSpeed - velocity;
}

math::Vec2 SteeringAgent::flee(math::Vec2 threat) const noexcept {
    return (position - threat).getNormalized() * maxSpeed - velocity;
}

math::Vec2 SteeringAgent::arrive(math::Vec2 target, float slowingRadius) const noexcept {
    const math::Vec2 offset = target - position;
    const float remaining = offset.getLength();
    if (remaining <= 0.0F) {
        return -velocity;
    }
    const float speed = slowingRadius > 0.0F ? maxSpeed * std::min(remaining / slowingRadius, 1.0F) : maxSpeed;
    return offset / remaining * speed - velocity;
}

math::Vec2 SteeringAgent::separation(std::span<const math::Vec2> neighbors, float radius) const noexcept {
    math::Vec2 force;
    for (const math::Vec2 neighbor : neighbors) {
        const math::Vec2 away = position - neighbor;
        const float gap = away.getLength();
        if (gap > 0.0F && gap < radius) {
            force += away / gap * (1.0F - gap / radius) * maxSpeed;
        }
    }
    return force;
}

math::Vec2 SteeringAgent::alignment(std::span<const math::Vec2> neighborVelocities) const noexcept {
    math::Vec2 heading;
    for (const math::Vec2 neighbor : neighborVelocities) {
        heading += neighbor;
    }
    if (heading.isZero()) {
        return {};
    }
    return heading.getNormalized() * maxSpeed - velocity;
}

math::Vec2 SteeringAgent::cohesion(std::span<const math::Vec2> neighborPositions) const noexcept {
    if (neighborPositions.empty()) {
        return {};
    }
    math::Vec2 center;
    for (const math::Vec2 neighbor : neighborPositions) {
        center += neighbor;
    }
    return seek(center / static_cast<float>(neighborPositions.size()));
}

math::Vec2 SteeringAgent::avoid(std::span<const math::Circle> obstacles, float lookAhead) const noexcept {
    const math::Vec2 heading = velocity.getNormalized();
    if (heading.isZero() || lookAhead <= 0.0F) {
        return {};
    }

    // The closest circle the path ahead enters is the threat, and the push points from its center across the heading.
    const math::Ray ahead{position, heading, lookAhead};
    const math::Circle* threat = nullptr;
    float closest = lookAhead;
    for (const math::Circle& obstacle : obstacles) {
        const std::optional<math::RayHit> hit = math::Raycast::circle(ahead, obstacle);
        if (hit && hit->distance <= closest) {
            closest = hit->distance;
            threat = &obstacle;
        }
    }
    if (threat == nullptr) {
        return {};
    }

    const math::Vec2 offset = position - threat->center;
    math::Vec2 side = offset - heading * math::Vec2::dot(offset, heading);
    if (side.isZero()) {
        side = heading.getPerpendicular();
    }
    return side.getNormalized() * (maxSpeed * (1.0F - closest / lookAhead));
}

void SteeringAgent::apply(math::Vec2 force, float deltaSeconds) noexcept {
    velocity = (velocity + force.clampedLength(maxForce * deltaSeconds)).clampedLength(maxSpeed);
    position += velocity * deltaSeconds;
}

} // namespace haylen::navigation2d
