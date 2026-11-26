#pragma once

#include <limits>

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A half-line in 3D that starts at the origin, runs along a unit direction and stops after its length. Casts measure their hits as distances from the origin.
struct Ray3D {
    Vec3 origin;
    Vec3 direction = Vec3::forward();
    float length = std::numeric_limits<float>::infinity();

    // Returns the ray from one point to the other, which points along `Vec3::forward()` with zero length when both points are equal.
    [[nodiscard]] static Ray3D between(Vec3 from, Vec3 to) noexcept {
        const Vec3 offset = to - from;
        const float distance = offset.getLength();
        return {from, distance > 0.0F ? offset / distance : Vec3::forward(), distance};
    }

    // Returns the ray that a camera sees through `point` of its view, in normalized device coordinates that run from -1 to 1 from left to right and from bottom to top, such as what lies under the pointer. The ray starts on the near plane and has no end, so it also reaches past a far plane at infinity. The range and direction of depth are those the projection was built with. Throws `std::invalid_argument` for a point that is not finite or a matrix that has no inverse or is not the projection and view of a camera.
    [[nodiscard]] static Ray3D fromView(const Mat4& viewProjection, Vec2 point, Mat4::DepthRange depthRange, bool reversedDepth);

    [[nodiscard]] constexpr Vec3 at(float distance) const noexcept {
        return origin + direction * distance;
    }
    [[nodiscard]] constexpr Vec3 getEndPoint() const noexcept {
        return at(length);
    }
};

} // namespace haylen::math
