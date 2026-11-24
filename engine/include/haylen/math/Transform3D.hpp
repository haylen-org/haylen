#pragma once

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// The placement of an object in 3D: it scales a point by `scale`, turns it by `rotation` and moves it by `position`, in that order.
struct Transform3D {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1.0F, 1.0F, 1.0F};

    [[nodiscard]] static constexpr Transform3D identity() noexcept {
        return {};
    }

    // Returns the parts of an affine matrix without shear, with the errors of `Mat4::decompose`.
    [[nodiscard]] static Transform3D fromMatrix(const Mat4& matrix);

    [[nodiscard]] constexpr bool operator==(const Transform3D&) const noexcept = default;

    // Returns the transform of a child of this transform, which places the child in the space of this one. The position of the child always lands where this transform puts it. The rotation and scale combine exactly while the scale of this transform is the same on every axis, negative or not, or the child does not turn. A child that turns under a scale that differs between axes would need a shear, which this form cannot hold, so such a chain multiplies the matrices of `toMatrix`.
    [[nodiscard]] Transform3D operator*(const Transform3D& child) const noexcept;

    [[nodiscard]] Vec3 apply(Vec3 point) const noexcept {
        return position + rotation.rotate(scale * point);
    }
    [[nodiscard]] Vec3 applyVector(Vec3 vector) const noexcept {
        return rotation.rotate(scale * vector);
    }

    // Returns the normal of a surface that the transform places, with a length of one, which stays at a right angle to the surface under a scale that differs between axes. Throws `std::invalid_argument` when the scale is zero on an axis.
    [[nodiscard]] Vec3 applyNormal(Vec3 normal) const;

    // Returns the point whose placement is `point`, which undoes the transform exactly at any scale. Throws `std::invalid_argument` when the scale is zero on an axis.
    [[nodiscard]] Vec3 applyInverse(Vec3 point) const;
    [[nodiscard]] Vec3 applyInverseVector(Vec3 vector) const;

    // Returns the transform that undoes this one. Throws `std::invalid_argument` when the scale is zero on an axis or differs between axes, since the undoing of such a scale has no exact transform of this form and needs the inverse of `toMatrix`.
    [[nodiscard]] Transform3D getInverse() const;

    [[nodiscard]] Mat4 toMatrix() const noexcept {
        return Mat4::compose(position, rotation, scale);
    }

    // Interpolates the position and the scale in a straight line and the rotation along the shorter arc.
    [[nodiscard]] static Transform3D lerp(const Transform3D& from, const Transform3D& to, float t) noexcept;

    [[nodiscard]] static bool approximately(const Transform3D& lhs, const Transform3D& rhs, float epsilon = 1e-5F) noexcept;

  private:
    void checkScale() const;
};

} // namespace haylen::math
