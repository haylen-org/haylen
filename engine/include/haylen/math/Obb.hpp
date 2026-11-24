#pragma once

#include "haylen/math/Box3.hpp"
#include "haylen/math/Mat3.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Transform3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// An oriented box: a box of `halfExtents` around `center`, turned by `rotation`.
struct Obb {
    Vec3 center;
    Quat rotation;
    Vec3 halfExtents;

    // Returns the box that `placement` makes of `box`, whose scale may be negative.
    [[nodiscard]] static Obb fromTransform(const Box3& box, const Transform3D& placement) noexcept;

    [[nodiscard]] constexpr bool operator==(const Obb&) const noexcept = default;

    // Returns the axes of the box as the columns of its rotation.
    [[nodiscard]] Mat3 getAxes() const noexcept {
        return rotation.toMatrix();
    }

    [[nodiscard]] bool contains(Vec3 point) const noexcept;
    [[nodiscard]] Vec3 getClosestPoint(Vec3 point) const noexcept;
    [[nodiscard]] Box3 getBounds() const noexcept;
};

} // namespace haylen::math
