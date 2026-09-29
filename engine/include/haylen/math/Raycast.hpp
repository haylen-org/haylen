#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "haylen/math/Circle.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/RayHit.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Casts rays against shapes without a physics world, and composes any cast into bouncing paths and fans. Circles, rectangles and polygons are solid, so a ray that starts inside one hits it at distance zero with a zero normal. Segments and chains are hit from both sides.
class Raycast final {
  public:
    // How far a bounce moves the next ray off the surface it reflects from, so the next cast never hits that surface again at its origin.
    static constexpr float kSurfaceOffset = 1e-3F;

    // Returns the distances along the ray where it enters and leaves the rectangle, limited to the ray, or nothing when the ray misses the rectangle.
    [[nodiscard]] static std::optional<std::array<float, 2>> clip(const Ray& ray, const Rect& area) noexcept;

    [[nodiscard]] static std::optional<RayHit> segment(const Ray& ray, const Segment& target) noexcept;
    [[nodiscard]] static std::optional<RayHit> rect(const Ray& ray, const Rect& target) noexcept;
    [[nodiscard]] static std::optional<RayHit> circle(const Ray& ray, const Circle& target) noexcept;

    // The polygon is simple and in either winding. The hit index is the edge that starts at that point index.
    [[nodiscard]] static std::optional<RayHit> polygon(const Ray& ray, std::span<const Vec2> points) noexcept;

    // Casts against the segments between consecutive points, plus the closing segment of a loop. The hit index is the first point of the segment.
    [[nodiscard]] static std::optional<RayHit> chain(const Ray& ray, std::span<const Vec2> points, bool loop) noexcept;

    // Returns the closest hit on any of the segments, whose index is the position of the segment in the list.
    [[nodiscard]] static std::optional<RayHit> segments(const Ray& ray, std::span<const Segment> targets) noexcept;

    // Fills hits with every segment the ray crosses, sorted by distance and then by index, keeping at most limit hits. A limit of zero keeps them all.
    static void segmentsAll(const Ray& ray, std::span<const Segment> targets, std::size_t limit, std::vector<RayHit>& hits);

    [[nodiscard]] static Vec2 reflect(Vec2 direction, Vec2 normal) noexcept {
        return direction - normal * (2.0F * Vec2::dot(direction, normal));
    }

    // Follows a ray that bounces off every surface it hits, like a laser between mirrors, for at most bounces reflections. Cast takes a Ray and returns an optional hit with point, normal and distance members. Hits receives every bounce point in order, and the returned ray is the last leg of the path, whose end is where the path stops. A hit with a zero normal stops the path.
    template <typename Hit, typename Cast> static Ray bounce(Ray ray, int bounces, Cast&& cast, std::vector<Hit>& hits) {
        hits.clear();
        for (int reflection = 0;; ++reflection) {
            const std::optional<Hit> hit = cast(ray);
            if (!hit) {
                return ray;
            }

            hits.push_back(*hit);
            if (reflection >= bounces || hit->normal.isZero()) {
                ray.length = hit->distance;
                return ray;
            }
            const Vec2 direction = reflect(ray.direction, hit->normal);
            ray = {hit->point + hit->normal * kSurfaceOffset, direction, ray.length - hit->distance};
        }
    }

    // Casts count rays of the given length, spread evenly across an arc of spread radians centered on angle, like a cone of vision. Results receives one optional hit per ray in order of increasing angle, and a single ray points straight along angle.
    template <typename Hit, typename Cast> static void fan(Vec2 origin, float angle, float spread, std::size_t count, float length, Cast&& cast, std::vector<std::optional<Hit>>& results) {
        results.clear();
        const float step = count > 1 ? spread / static_cast<float>(count - 1) : 0.0F;
        const float first = count > 1 ? angle - spread * 0.5F : angle;
        for (std::size_t index = 0; index < count; ++index) {
            results.push_back(cast(Ray::fromAngle(origin, first + step * static_cast<float>(index), length)));
        }
    }

  private:
    // Returns the unit normal of an edge that faces against the ray direction.
    [[nodiscard]] static Vec2 facingNormal(Vec2 edge, Vec2 direction) noexcept;

    // Returns the distance along the ray where it crosses the segment from start to end, including both ends.
    [[nodiscard]] static std::optional<float> crossing(const Ray& ray, Vec2 start, Vec2 end) noexcept;
};

} // namespace haylen::math
