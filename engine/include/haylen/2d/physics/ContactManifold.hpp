#pragma once

#include <array>
#include <cstddef>

#include "haylen/2d/physics/ContactPoint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// Everything a contact between two shapes did in the last step, in world units: up to two points, the normal from the first shape toward the second, so the second was hit from the side the normal points away from, the velocity of the second body relative to the first at the first point after the step, the fastest approach over the points, which tells how hard an impact was, and the force that presses the shapes together at the end of the step, which is the weight of a body at rest.
struct ContactManifold {
    static constexpr std::size_t kMaxPoints = 2;

    std::array<ContactPoint, kMaxPoints> points{};
    std::size_t pointCount = 0;
    math::Vec2 normal{};
    math::Vec2 relativeVelocity{};
    float approachSpeed = 0.0F;

    // The normal impulses of the last sub-step over its length, in kilograms times units per second squared.
    float force = 0.0F;

    // The first point, where the contact touches, or the origin without points.
    [[nodiscard]] math::Vec2 getPoint() const noexcept {
        return pointCount > 0 ? points[0].point : math::Vec2{};
    }
};

} // namespace haylen::physics2d
