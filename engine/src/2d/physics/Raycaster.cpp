#include "haylen/2d/physics/Raycaster.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Raycast.hpp"

namespace haylen::physics2d {

const Raycaster::Filter Raycaster::kDefaultFilter{};

// Collects the hits of one cast from inside Box2D, either the closest one alone or all of them.
struct Raycaster::Gather {
    World* world = nullptr;
    float scale = 1.0F;
    CollisionFilter filter{};
    float length = 0.0F;
    bool closest = true;
    bool skipOverlap = true;
    std::optional<RaycastHit> best;
    std::vector<RaycastHit> hits;

    // Box2D prefilters by category and mask, unless a group asks for the full rule, which report applies itself.
    [[nodiscard]] b2QueryFilter getQueryFilter() const noexcept {
        return filter.group == 0 ? Box2DConverter::toQueryFilter(filter) : b2QueryFilter{.categoryBits = ~std::uint64_t{0}, .maskBits = ~std::uint64_t{0}};
    }

    static float report(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
        auto& gather = *static_cast<Gather*>(context);
        if (gather.skipOverlap && fraction == 0.0F) {
            return -1.0F;
        }
        if (gather.filter.group != 0) {
            const b2Filter shape = b2Shape_GetFilter(shapeId);
            const bool seen = shape.groupIndex == gather.filter.group ? gather.filter.group > 0 : (shape.categoryBits & gather.filter.mask) != 0 && (shape.maskBits & gather.filter.category) != 0;
            if (!seen) {
                return -1.0F;
            }
        }

        const RaycastHit hit{.shape = {gather.world, b2StoreShapeId(shapeId)}, .point = Box2DConverter::toPixels(point, gather.scale), .normal = {normal.x, normal.y}, .fraction = fraction, .distance = fraction * gather.length};
        if (gather.closest) {
            gather.best = hit;
            return fraction;
        }
        gather.hits.push_back(hit);
        return 1.0F;
    }
};

template <typename Cast> std::optional<RaycastHit> Raycaster::firstAccepted(const Filter& filter, Cast&& cast) const {
    Gather gather{.filter = filter.collision, .closest = !filter.accept};
    cast(gather);
    if (!filter.accept) {
        return gather.best;
    }

    // Accept runs after Box2D finished, in order from the start of the cast, so it may even change the world.
    std::ranges::sort(gather.hits, [](const RaycastHit& lhs, const RaycastHit& rhs) { return lhs.fraction != rhs.fraction ? lhs.fraction < rhs.fraction : lhs.shape.getId() < rhs.shape.getId(); });
    for (const RaycastHit& hit : gather.hits) {
        if (filter.accept(hit)) {
            return hit;
        }
    }
    return std::nullopt;
}

void Raycaster::castRayInto(math::Vec2 from, math::Vec2 to, Gather& gather) const {
    gather.world = const_cast<World*>(&world);
    gather.scale = world.getPixelsPerMeter();
    gather.length = math::Vec2::distance(from, to);
    b2World_CastRay(b2LoadWorldId(world.getHandle()), Box2DConverter::toMeters(from, gather.scale), Box2DConverter::toMeters(to - from, gather.scale), gather.getQueryFilter(), &Gather::report, &gather);
}

std::optional<RaycastHit> Raycaster::castRay(math::Vec2 from, math::Vec2 to, const Filter& filter) const {
    return firstAccepted(filter, [&](Gather& gather) { castRayInto(from, to, gather); });
}

void Raycaster::castRayAll(math::Vec2 from, math::Vec2 to, const Filter& filter, std::size_t limit, std::vector<RaycastHit>& hits) const {
    Gather gather{.filter = filter.collision, .closest = false};
    castRayInto(from, to, gather);
    std::ranges::sort(gather.hits, [](const RaycastHit& lhs, const RaycastHit& rhs) { return lhs.fraction != rhs.fraction ? lhs.fraction < rhs.fraction : lhs.shape.getId() < rhs.shape.getId(); });

    hits.clear();
    for (const RaycastHit& hit : gather.hits) {
        if (limit > 0 && hits.size() == limit) {
            return;
        }
        if (!filter.accept || filter.accept(hit)) {
            hits.push_back(hit);
        }
    }
}

std::optional<RaycastHit> Raycaster::castShape(std::span<const math::Vec2> points, float radius, math::Vec2 translation, const Filter& filter) const {
    const float scale = world.getPixelsPerMeter();
    std::array<b2Vec2, B2_MAX_POLYGON_VERTICES> corners{};
    for (std::size_t index = 0; index < points.size(); ++index) {
        corners[index] = Box2DConverter::toMeters(points[index], scale);
    }
    const b2ShapeProxy proxy = b2MakeProxy(corners.data(), static_cast<int>(points.size()), radius / scale);

    // clang-format off
    return firstAccepted(filter, [&](Gather& gather) {
        gather.world = const_cast<World*>(&world);
        gather.scale = scale;
        gather.length = translation.getLength();
        gather.skipOverlap = false;
        b2World_CastShape(b2LoadWorldId(world.getHandle()), &proxy, Box2DConverter::toMeters(translation, scale), gather.getQueryFilter(), &Gather::report, &gather);
    });
    // clang-format on
}

std::optional<RaycastHit> Raycaster::castCircle(math::Vec2 center, float radius, math::Vec2 translation, const Filter& filter) const {
    if (!(radius >= 0.0F) || !std::isfinite(radius)) {
        throw std::invalid_argument("A circle cast needs a finite radius of zero or more.");
    }
    const std::array<math::Vec2, 1> points{center};
    return castShape(points, radius, translation, filter);
}

std::optional<RaycastHit> Raycaster::castBox(math::Vec2 center, math::Vec2 size, float rotation, math::Vec2 translation, const Filter& filter) const {
    if (!(size.x > 0.0F) || !(size.y > 0.0F) || !std::isfinite(size.x) || !std::isfinite(size.y)) {
        throw std::invalid_argument("A box cast needs a positive size.");
    }
    const math::Vec2 half = size * 0.5F;
    const std::array<math::Vec2, 4> points{center + math::Vec2{-half.x, -half.y}.rotated(rotation), center + math::Vec2{half.x, -half.y}.rotated(rotation), center + half.rotated(rotation), center + math::Vec2{-half.x, half.y}.rotated(rotation)};
    return castShape(points, 0.0F, translation, filter);
}

std::optional<RaycastHit> Raycaster::castCapsule(math::Vec2 first, math::Vec2 second, float radius, math::Vec2 translation, const Filter& filter) const {
    if (!(radius > 0.0F) || !std::isfinite(radius)) {
        throw std::invalid_argument("A capsule cast needs a positive radius.");
    }
    const std::array<math::Vec2, 2> points{first, second};
    return castShape(points, radius, translation, filter);
}

std::optional<RaycastHit> Raycaster::castPolygon(std::span<const math::Vec2> points, math::Vec2 translation, const Filter& filter) const {
    if (points.size() < 3 || points.size() > B2_MAX_POLYGON_VERTICES) {
        throw std::invalid_argument("A polygon cast needs between three and eight points.");
    }
    return castShape(points, 0.0F, translation, filter);
}

math::Ray Raycaster::bounce(const math::Ray& ray, int bounces, const Filter& filter, std::vector<RaycastHit>& hits) const {
    if (!std::isfinite(ray.length)) {
        throw std::invalid_argument("A bouncing physics ray needs a finite length.");
    }
    return math::Raycast::bounce(ray, bounces, [this, &filter](const math::Ray& leg) { return castRay(leg.origin, leg.getEnd(), filter); }, hits);
}

void Raycaster::fan(math::Vec2 origin, float angle, float spread, std::size_t count, float length, const Filter& filter, std::vector<std::optional<RaycastHit>>& results) const {
    if (!std::isfinite(length)) {
        throw std::invalid_argument("A fan of physics rays needs a finite length.");
    }
    math::Raycast::fan(origin, angle, spread, count, length, [this, &filter](const math::Ray& ray) { return castRay(ray.origin, ray.getEnd(), filter); }, results);
}

bool Raycaster::hasLineOfSight(math::Vec2 from, math::Vec2 to, const Filter& filter) const {
    return !castRay(from, to, filter).has_value();
}

void Raycaster::castBatch(RayBatch& batch, const CollisionFilter& filter, core::JobSystem* jobs) const {
    // clang-format off
    const auto castRange = [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            Gather gather{.filter = filter};
            castRayInto(batch.rays[index].start, batch.rays[index].end, gather);
            batch.results[index] = gather.best;
        }
    };
    // clang-format on
    if (jobs == nullptr) {
        castRange(0, batch.size());
        return;
    }
    jobs->parallelFor(0, batch.size(), kBatchGrain, castRange);
}

} // namespace haylen::physics2d
