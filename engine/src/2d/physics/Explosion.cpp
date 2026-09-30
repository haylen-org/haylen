#include "haylen/2d/physics/Explosion.hpp"

#include <algorithm>
#include <array>
#include <box2d/box2d.h>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Explosion::Falloff>, 3> Explosion::kFalloffNames{{{"none", Falloff::None}, {"linear", Falloff::Linear}, {"quadratic", Falloff::Quadratic}}};

std::optional<Explosion::Falloff> Explosion::falloffFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFalloffNames, name, &std::pair<std::string_view, Falloff>::first);
    return found != kFalloffNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Explosion::falloffName(Falloff value) noexcept {
    return std::ranges::find(kFalloffNames, value, &std::pair<std::string_view, Falloff>::second)->first;
}

float Explosion::scaleAt(const Options& options, float distance) noexcept {
    const float remaining = math::Math::saturate(1.0F - distance / options.radius);
    switch (options.falloff) {
    case Falloff::None:
        return 1.0F;
    case Falloff::Quadratic:
        return remaining * remaining;
    case Falloff::Linear:
        break;
    }
    return remaining;
}

std::vector<Explosion::Hit> Explosion::apply(World& world, const Options& options) {
    if (options.radius <= 0.0F) {
        throw std::invalid_argument("An explosion needs a positive radius.");
    }

    // Every dynamic body keeps the point of its shapes closest to the center.
    const float scale = world.getPixelsPerMeter();
    const b2Vec2 center = Box2DConverter::toMeters(options.center, scale);
    std::vector<Hit> hits;
    std::vector<float> distances;
    for (const Shape& shape : world.queryCircle(options.center, options.radius, options.filter)) {
        const Body body = shape.getBody();
        if (shape.isSensor() || body.getType() != Body::Type::Dynamic) {
            continue;
        }

        const math::Vec2 point = Box2DConverter::toPixels(b2Shape_GetClosestPoint(b2LoadShapeId(shape.getId()), center), scale);
        const float distance = math::Vec2::distance(point, options.center);
        const auto known = std::find_if(hits.begin(), hits.end(), [&body](const Hit& hit) { return hit.body == body; });
        if (known == hits.end()) {
            hits.push_back({body, point, {}});
            distances.push_back(distance);
        } else if (distance < distances[static_cast<std::size_t>(known - hits.begin())]) {
            known->point = point;
            distances[static_cast<std::size_t>(known - hits.begin())] = distance;
        }
    }

    // Only solid shapes that pass the filter of the blast shield the bodies behind them.
    const Raycaster::Filter shield{.collision = options.filter, .accept = [](const RaycastHit& candidate) { return !candidate.shape.isSensor(); }};
    std::vector<Hit> pushed;
    for (std::size_t index = 0; index < hits.size(); ++index) {
        Hit& hit = hits[index];
        if (options.occlusion) {
            const std::optional<RaycastHit> blocker = Raycaster(world).castRay(options.center, hit.point, shield);
            if (blocker && !(blocker->shape.getBody() == hit.body)) {
                continue;
            }
        }

        // A center inside the body pushes from its center of mass, and a center right on it pushes up.
        math::Vec2 direction = hit.point - options.center;
        if (direction.isZero()) {
            direction = Box2DConverter::toPixels(b2Body_GetWorldCenterOfMass(b2LoadBodyId(hit.body.getId())), scale) - options.center;
        }
        if (direction.isZero()) {
            direction = {0.0F, -1.0F};
        }

        hit.impulse = direction.getNormalized() * (options.impulse * scaleAt(options, distances[index]));
        hit.body.applyImpulse(hit.impulse, hit.point);
        pushed.push_back(hit);
    }
    return pushed;
}

} // namespace haylen::physics2d
