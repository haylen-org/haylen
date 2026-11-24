#pragma once

#include <span>

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A box whose faces line up with the axes, from its smallest corner `min` to its largest corner `max`. Its edges count as part of it.
struct Box {
    Vec3 min;
    Vec3 max;

    [[nodiscard]] static constexpr Box fromCenter(Vec3 middle, Vec3 extent) noexcept {
        return {middle - extent * 0.5F, middle + extent * 0.5F};
    }

    // Returns the smallest box that holds every point. Throws `std::invalid_argument` for no points.
    [[nodiscard]] static Box bounds(std::span<const Vec3> points);

    [[nodiscard]] constexpr bool operator==(const Box&) const noexcept = default;

    [[nodiscard]] constexpr Vec3 getCenter() const noexcept {
        return (min + max) * 0.5F;
    }
    [[nodiscard]] constexpr Vec3 getSize() const noexcept {
        return max - min;
    }
    [[nodiscard]] constexpr Vec3 getHalfExtents() const noexcept {
        return (max - min) * 0.5F;
    }
    // A box is empty when its largest corner lies below its smallest corner on an axis.
    [[nodiscard]] constexpr bool isEmpty() const noexcept {
        return max.x < min.x || max.y < min.y || max.z < min.z;
    }

    [[nodiscard]] constexpr bool contains(Vec3 point) const noexcept {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z && point.z <= max.z;
    }
    [[nodiscard]] constexpr bool contains(const Box& other) const noexcept {
        return other.min.x >= min.x && other.max.x <= max.x && other.min.y >= min.y && other.max.y <= max.y && other.min.z >= min.z && other.max.z <= max.z;
    }
    // Two boxes intersect when they overlap or touch.
    [[nodiscard]] constexpr bool intersects(const Box& other) const noexcept {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y && min.z <= other.max.z && max.z >= other.min.z;
    }

    [[nodiscard]] constexpr Box merged(const Box& other) const noexcept {
        return {Vec3::min(min, other.min), Vec3::max(max, other.max)};
    }
    [[nodiscard]] constexpr Box merged(Vec3 point) const noexcept {
        return {Vec3::min(min, point), Vec3::max(max, point)};
    }
    [[nodiscard]] constexpr Box expanded(float amount) const noexcept {
        return {min - Vec3{amount, amount, amount}, max + Vec3{amount, amount, amount}};
    }
    [[nodiscard]] constexpr Vec3 clamp(Vec3 point) const noexcept {
        return Vec3::min(Vec3::max(point, min), max);
    }

    // Returns the smallest box that holds this box after an affine matrix places it, built from the extents that each column of the matrix spans. Throws `std::invalid_argument` for a matrix that is not affine.
    [[nodiscard]] Box transformed(const Mat4& placement) const;
};

} // namespace haylen::math
