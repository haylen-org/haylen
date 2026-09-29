#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/2d/physics/RaycastHit.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics2d {

class World;

// Casts rays and swept shapes through a physics world in world units: the closest hit, every hit in order with an optional limit, bouncing rays, fans of rays, lines of sight and batches spread over the job system. Accept functions never run inside Box2D, so they may run any code, including Lua.
class Raycaster final {
  public:
    // A cast sees a shape when the shape category shares a bit with the mask and the shape mask shares a bit with the category, like a collision. A shape that shares a nonzero group with the filter is always seen when the group is positive and never when it is negative. Accept, when set, then decides about each hit in order from the start of the cast.
    struct Filter {
        CollisionFilter collision{};
        std::function<bool(const RaycastHit&)> accept;
    };

    explicit Raycaster(const World& target) noexcept : world(target) {}

    // Returns the first hit that passes the filter, or nothing. A ray that starts inside a shape does not see it.
    [[nodiscard]] std::optional<RaycastHit> castRay(math::Vec2 from, math::Vec2 to, const Filter& filter = kDefaultFilter) const;

    // Fills hits with the hits that pass the filter in order from the start of the ray, and stops after limit hits unless the limit is zero, so a ray can pierce a number of shapes.
    void castRayAll(math::Vec2 from, math::Vec2 to, const Filter& filter, std::size_t limit, std::vector<RaycastHit>& hits) const;

    // Shape casts sweep a shape along the translation and return the first hit that passes the filter, where the fraction tells how far the shape travels before it touches. A shape that starts touching another one hits it at fraction 0 with a zero normal.
    [[nodiscard]] std::optional<RaycastHit> castCircle(math::Vec2 center, float radius, math::Vec2 translation, const Filter& filter = kDefaultFilter) const;
    [[nodiscard]] std::optional<RaycastHit> castBox(math::Vec2 center, math::Vec2 size, float rotation, math::Vec2 translation, const Filter& filter = kDefaultFilter) const;
    [[nodiscard]] std::optional<RaycastHit> castCapsule(math::Vec2 first, math::Vec2 second, float radius, math::Vec2 translation, const Filter& filter = kDefaultFilter) const;

    // Sweeps the convex hull of up to eight points.
    [[nodiscard]] std::optional<RaycastHit> castPolygon(std::span<const math::Vec2> points, math::Vec2 translation, const Filter& filter = kDefaultFilter) const;

    // Follows a ray of finite length that bounces off every shape it hits, like a laser between mirrors, for at most bounces reflections. Hits receives each bounce with its distance along its own leg, and the returned ray is the last leg, whose end is where the path stops.
    math::Ray bounce(const math::Ray& ray, int bounces, const Filter& filter, std::vector<RaycastHit>& hits) const;

    // Casts count rays of the given length spread evenly across an arc of spread radians centered on angle, like a cone of vision, with one optional hit per ray in order of increasing angle.
    void fan(math::Vec2 origin, float angle, float spread, std::size_t count, float length, const Filter& filter, std::vector<std::optional<RaycastHit>>& results) const;

    // Returns true when no shape that passes the filter blocks the segment between both points.
    [[nodiscard]] bool hasLineOfSight(math::Vec2 from, math::Vec2 to, const Filter& filter = kDefaultFilter) const;

    // Stores the closest hit of every ray of the batch, spread over the workers of the job system when one is given. Batches filter by category, mask and group only, because their rays run on worker threads.
    void castBatch(RayBatch& batch, const CollisionFilter& filter = {}, core::JobSystem* jobs = nullptr) const;

  private:
    struct Gather;

    static const Filter kDefaultFilter;

    // Rays of a batch run in chunks of at least this many rays per worker.
    static constexpr std::size_t kBatchGrain = 64;

    // Returns the first accepted hit of a cast that gathers into the context it receives.
    template <typename Cast> [[nodiscard]] std::optional<RaycastHit> firstAccepted(const Filter& filter, Cast&& cast) const;
    void castRayInto(math::Vec2 from, math::Vec2 to, Gather& gather) const;
    [[nodiscard]] std::optional<RaycastHit> castShape(std::span<const math::Vec2> points, float radius, math::Vec2 translation, const Filter& filter) const;

    const World& world;
};

} // namespace haylen::physics2d
