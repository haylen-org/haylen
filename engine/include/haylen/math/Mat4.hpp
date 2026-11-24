#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Mat3.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Plane.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::math {

// A 4x4 matrix stored by columns, which multiplies a column vector on its right, so the matrices of a draw chain as `projection * view * model`.
struct Mat4 {
    // The range of depth in the clip space of a graphics API: `0` to `1` on Metal, Direct3D, Vulkan and WebGPU, and `-1` to `1` on OpenGL and WebGL2.
    enum class DepthRange : std::uint8_t {
        ZeroToOne,
        NegativeOneToOne,
    };

    // A projection with reversed depth puts the near plane at the far end of the range and the far plane at the near end, which keeps far surfaces apart in a depth buffer of floats.
    struct Perspective {
        float fieldOfView = Math::kPi / 3.0F;
        float aspect = 1.0F;
        float nearPlane = 0.1F;
        float farPlane = std::numeric_limits<float>::infinity();
        DepthRange depthRange = DepthRange::ZeroToOne;
        bool reversedDepth = true;
    };

    // The edges of the view as angles from the direction it looks along, negative to the left and below, such as the view of one eye of a headset.
    struct AsymmetricPerspective {
        float left = -Math::kPi / 4.0F;
        float right = Math::kPi / 4.0F;
        float bottom = -Math::kPi / 4.0F;
        float top = Math::kPi / 4.0F;
        float nearPlane = 0.1F;
        float farPlane = std::numeric_limits<float>::infinity();
        DepthRange depthRange = DepthRange::ZeroToOne;
        bool reversedDepth = true;
    };

    struct Orthographic {
        float left = -1.0F;
        float right = 1.0F;
        float bottom = -1.0F;
        float top = 1.0F;
        float nearPlane = 0.0F;
        float farPlane = 100.0F;
        DepthRange depthRange = DepthRange::ZeroToOne;
        bool reversedDepth = true;
    };

    // The parts of an affine matrix without shear, which `compose` puts back together.
    struct Decomposition {
        Vec3 translation;
        Quat rotation;
        Vec3 scale{1.0F, 1.0F, 1.0F};
    };

    // The element of row `r` and column `c` sits at `values[c * 4 + r]`.
    std::array<float, 16> values{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    [[nodiscard]] static constexpr Mat4 identity() noexcept {
        return {};
    }
    [[nodiscard]] static constexpr Mat4 fromColumns(Vec4 x, Vec4 y, Vec4 z, Vec4 w) noexcept {
        return {{x.x, x.y, x.z, x.w, y.x, y.y, y.z, y.w, z.x, z.y, z.z, z.w, w.x, w.y, w.z, w.w}};
    }
    [[nodiscard]] static constexpr Mat4 fromLinear(const Mat3& linear, Vec3 translation) noexcept {
        return fromColumns(Vec4::fromVec3(linear.getColumn(0), 0.0F), Vec4::fromVec3(linear.getColumn(1), 0.0F), Vec4::fromVec3(linear.getColumn(2), 0.0F), Vec4::fromVec3(translation, 1.0F));
    }

    // Returns the matrix that scales, then rotates, then translates.
    [[nodiscard]] static Mat4 compose(Vec3 translation, const Quat& rotation, Vec3 scale) noexcept;

    // Returns the view matrix of a camera at `eye` that looks at `target` with its up axis close to `up`. Throws `std::invalid_argument` when the eye and the target are one point or `up` runs along the view.
    [[nodiscard]] static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up = Vec3::up());

    // Each projection throws `std::invalid_argument` with the option that is out of range.
    [[nodiscard]] static Mat4 perspective(const Perspective& options);
    [[nodiscard]] static Mat4 asymmetricPerspective(const AsymmetricPerspective& options);
    [[nodiscard]] static Mat4 orthographic(const Orthographic& options);

    // Returns the matrix that turns the clip space of one range of depth into the other when it multiplies a projection on its left.
    [[nodiscard]] static constexpr Mat4 depthRangeConversion(DepthRange from, DepthRange to) noexcept {
        if (from == to) {
            return {};
        }

        if (from == DepthRange::ZeroToOne) {
            return {{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, -1.0F, 1.0F}};
        }

        return {{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.5F, 0.0F, 0.0F, 0.0F, 0.5F, 1.0F}};
    }

    [[nodiscard]] static std::optional<DepthRange> depthRangeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view depthRangeName(DepthRange value) noexcept;

    [[nodiscard]] constexpr bool operator==(const Mat4&) const noexcept = default;

    [[nodiscard]] constexpr float get(std::size_t row, std::size_t column) const noexcept {
        return values[column * 4 + row];
    }
    [[nodiscard]] constexpr Vec4 getColumn(std::size_t column) const noexcept {
        return {values[column * 4], values[column * 4 + 1], values[column * 4 + 2], values[column * 4 + 3]};
    }
    [[nodiscard]] constexpr Vec4 getRow(std::size_t row) const noexcept {
        return {values[row], values[4 + row], values[8 + row], values[12 + row]};
    }
    [[nodiscard]] constexpr Vec3 getTranslation() const noexcept {
        return {values[12], values[13], values[14]};
    }
    // Returns the upper left 3x3 matrix, the rotation, scale and shear of an affine matrix.
    [[nodiscard]] constexpr Mat3 getLinear() const noexcept {
        return Mat3::fromColumns(getColumn(0).getXyz(), getColumn(1).getXyz(), getColumn(2).getXyz());
    }

    [[nodiscard]] constexpr Mat4 operator*(const Mat4& rhs) const noexcept {
        Mat4 product;
        for (std::size_t column = 0; column < 4; ++column) {
            const Vec4 mapped = *this * rhs.getColumn(column);
            product.values[column * 4] = mapped.x;
            product.values[column * 4 + 1] = mapped.y;
            product.values[column * 4 + 2] = mapped.z;
            product.values[column * 4 + 3] = mapped.w;
        }

        return product;
    }

    [[nodiscard]] constexpr Vec4 operator*(Vec4 vector) const noexcept {
        return getColumn(0) * vector.x + getColumn(1) * vector.y + getColumn(2) * vector.z + getColumn(3) * vector.w;
    }

    // Returns the point that the matrix maps `point` to, divided by its `w`, so a projection gives the normalized device coordinates of the point.
    [[nodiscard]] constexpr Vec3 apply(Vec3 point) const noexcept {
        const Vec4 mapped = *this * Vec4::fromVec3(point, 1.0F);
        return mapped.getXyz() / mapped.w;
    }

    // Returns the direction that the matrix maps `vector` to, which turns and scales but never translates.
    [[nodiscard]] constexpr Vec3 applyVector(Vec3 vector) const noexcept {
        return (*this * Vec4::fromVec3(vector, 0.0F)).getXyz();
    }

    [[nodiscard]] constexpr Mat4 getTransposed() const noexcept {
        return fromColumns(getRow(0), getRow(1), getRow(2), getRow(3));
    }

    [[nodiscard]] float getDeterminant() const noexcept;

    // An affine matrix is invertible when its linear part is, by the test of `Mat3::isInvertible`, and any other matrix when its determinant is a finite number other than zero.
    [[nodiscard]] bool isInvertible() const noexcept;

    // An affine matrix has the last row `(0, 0, 0, 1)`, so it never divides by `w`.
    [[nodiscard]] constexpr bool isAffine() const noexcept {
        return values[3] == 0.0F && values[7] == 0.0F && values[11] == 0.0F && values[15] == 1.0F;
    }

    // Throws `std::invalid_argument` for a singular matrix, which has no inverse.
    [[nodiscard]] Mat4 getInverse() const;

    // Returns the inverse of an affine matrix with fewer operations than `getInverse`. Throws `std::invalid_argument` for a matrix that is not affine or whose linear part is singular.
    [[nodiscard]] Mat4 getAffineInverse() const;

    // Returns the matrix that maps normals the way this matrix maps the surfaces they belong to, the transposed inverse of its linear part. Throws `std::invalid_argument` when the linear part is singular.
    [[nodiscard]] Mat3 getNormalMatrix() const;

    // Splits an affine matrix into translation, rotation and scale. A mirrored matrix, whose determinant is negative, gets a negative scale on x. Throws `std::invalid_argument` for a matrix that is not affine or that scales an axis to zero.
    [[nodiscard]] Decomposition decompose() const;

    // Returns the projection with its near plane replaced by `plane` of the space of the camera, so nothing in front of the camera and behind the plane draws, such as the scene behind a mirror or a portal. The plane must face away from the camera, with the camera behind it, and cross the view before its far end. The range and direction of depth are those the projection was built with. Throws `std::invalid_argument` for a plane that does not cross the view that way or a projection that is singular.
    [[nodiscard]] Mat4 getObliqueNearPlane(const Plane& plane, DepthRange depthRange, bool reversedDepth) const;

    [[nodiscard]] static bool approximately(const Mat4& lhs, const Mat4& rhs, float epsilon = 1e-5F) noexcept;

  private:
    static const std::array<std::pair<std::string_view, DepthRange>, 2> kDepthRangeNames;

    // The depth that a projection gives its near plane and its far plane, for the range and direction of depth.
    [[nodiscard]] static std::pair<float, float> getDepthEnds(DepthRange depthRange, bool reversedDepth) noexcept;
    [[nodiscard]] static Mat4 projectTangents(float left, float right, float bottom, float top, float nearPlane, float farPlane, DepthRange depthRange, bool reversedDepth) noexcept;
    // The twelve products of pairs of elements that the determinant and the inverse share.
    [[nodiscard]] std::array<float, 12> getMinors() const noexcept;
    static void checkDepthPlanes(float nearPlane, float farPlane, std::string_view projection);
};

} // namespace haylen::math
