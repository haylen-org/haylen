#pragma once

#include <optional>
#include <utility>

#include "haylen/math/Box.hpp"
#include "haylen/math/Capsule.hpp"
#include "haylen/math/Obb.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/RayHit3D.hpp"
#include "haylen/math/Segment3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Triangle.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// Ray casts, overlap tests and closest points of the 3D shapes. Boxes, oriented boxes, spheres and capsules are solid, so a ray that starts inside one hits it at distance zero with a zero normal. Triangles and planes are hit from both sides.
class Geometry3D final {
  public:
    // Hits a triangle with the watertight test, which never lets a ray slip between two triangles that share an edge.
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Triangle& triangle) noexcept;
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Box& box) noexcept;
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Obb& box) noexcept;
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Sphere& sphere) noexcept;
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Plane& plane) noexcept;
    [[nodiscard]] static std::optional<RayHit3D> raycast(const Ray3D& ray, const Capsule& capsule) noexcept;

    // Tests two oriented boxes on the fifteen axes that can separate them.
    [[nodiscard]] static bool intersects(const Obb& lhs, const Obb& rhs) noexcept;

    // Returns the closest points of two segments, on the first and on the second.
    [[nodiscard]] static std::pair<Vec3, Vec3> closestPoints(const Segment3D& lhs, const Segment3D& rhs) noexcept;

  private:
    // Rays that run along a face of a box closer than this stay outside, so a ray that grazes an edge never divides by zero.
    static constexpr float kParallel = 1e-12F;

    [[nodiscard]] static bool isWithin(const Ray3D& ray, float distance) noexcept {
        return distance >= 0.0F && distance <= ray.length;
    }
    [[nodiscard]] static std::optional<RayHit3D> raycastSphere(const Ray3D& ray, Vec3 center, float radius) noexcept;
};

} // namespace haylen::math
