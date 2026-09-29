#include "haylen/2d/navigation/Wanderer.hpp"

namespace haylen::navigation2d {

const Wanderer::Settings Wanderer::kDefaultSettings{};

Wanderer::Wanderer(std::uint64_t seed, const Settings& value) : random(seed), settings(value) {}

math::Vec2 Wanderer::steer(const SteeringAgent& agent, float deltaSeconds) noexcept {
    angle += random.range(-1.0F, 1.0F) * settings.jitter * deltaSeconds;
    const float heading = agent.velocity.isZero() ? 0.0F : agent.velocity.getAngle();
    const math::Vec2 ahead = agent.position + math::Vec2::fromAngle(heading, settings.distance);
    return agent.seek(ahead + math::Vec2::fromAngle(heading + angle, settings.radius));
}

} // namespace haylen::navigation2d
