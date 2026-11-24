#include "haylen/math/Obb.hpp"

#include <cmath>

namespace haylen::math {

Obb Obb::fromTransform(const Box& box, const Transform3D& placement) noexcept {
    const Vec3 size{std::fabs(placement.scale.x), std::fabs(placement.scale.y), std::fabs(placement.scale.z)};
    return {placement.apply(box.getCenter()), placement.rotation, box.getHalfExtents() * size};
}

bool Obb::contains(Vec3 point) const noexcept {
    const Quat back{-rotation.x, -rotation.y, -rotation.z, rotation.w};
    const Vec3 local = back.rotate(point - center);
    return std::fabs(local.x) <= halfExtents.x && std::fabs(local.y) <= halfExtents.y && std::fabs(local.z) <= halfExtents.z;
}

Vec3 Obb::getClosestPoint(Vec3 point) const noexcept {
    const Quat back{-rotation.x, -rotation.y, -rotation.z, rotation.w};
    const Vec3 local = back.rotate(point - center);
    const Vec3 clamped = Vec3::min(Vec3::max(local, -halfExtents), halfExtents);
    return center + rotation.rotate(clamped);
}

// Each axis of the box reaches along every axis of the world by the size of its component there.
Box Obb::getBounds() const noexcept {
    const Mat3 axes = getAxes();
    Vec3 reach;
    for (std::size_t column = 0; column < 3; ++column) {
        const Vec3 axis = axes.getColumn(column);
        const float extent = column == 0 ? halfExtents.x : (column == 1 ? halfExtents.y : halfExtents.z);
        reach += Vec3{std::fabs(axis.x), std::fabs(axis.y), std::fabs(axis.z)} * extent;
    }

    return {center - reach, center + reach};
}

} // namespace haylen::math
