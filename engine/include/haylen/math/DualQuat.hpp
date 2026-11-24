#pragma once

#include <span>

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A rotation and a translation in one dual quaternion, which blends the joints of a skinned mesh without the loss of volume that blending matrices gives at twisted joints.
struct DualQuat {
    Quat real;
    Quat dual{0.0F, 0.0F, 0.0F, 0.0F};

    [[nodiscard]] static constexpr DualQuat identity() noexcept {
        return {};
    }

    // Returns the dual quaternion that turns by `rotation` and then moves by `translation`.
    [[nodiscard]] static DualQuat fromRotationTranslation(const Quat& rotation, Vec3 translation) noexcept;

    // Blends dual quaternions by their weights along the shorter arc of each, as the joints of a vertex do. Throws `std::invalid_argument` for lists of different lengths, an empty list, weights that are negative or not finite, or weights that add up to zero.
    [[nodiscard]] static DualQuat blend(std::span<const DualQuat> parts, std::span<const float> weights);

    [[nodiscard]] constexpr bool operator==(const DualQuat&) const noexcept = default;

    // Returns the dual quaternion that applies `rhs` first and this one after it.
    [[nodiscard]] constexpr DualQuat operator*(const DualQuat& rhs) const noexcept {
        const Quat realPart = real * rhs.real;
        const Quat left = real * rhs.dual;
        const Quat right = dual * rhs.real;
        return {realPart, {left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w}};
    }

    [[nodiscard]] Quat getRotation() const noexcept {
        return real;
    }
    [[nodiscard]] Vec3 getTranslation() const noexcept;

    [[nodiscard]] Vec3 apply(Vec3 point) const noexcept {
        return real.rotate(point) + getTranslation();
    }

    // Returns the dual quaternion with a real part of length one. Throws `std::invalid_argument` when the real part has a length of zero or one that is not finite.
    [[nodiscard]] DualQuat getNormalized() const;

    [[nodiscard]] Mat4 toMatrix() const noexcept {
        return Mat4::compose(getTranslation(), real, {1.0F, 1.0F, 1.0F});
    }

    [[nodiscard]] static bool approximately(const DualQuat& lhs, const DualQuat& rhs, float epsilon = 1e-5F) noexcept;
};

} // namespace haylen::math
