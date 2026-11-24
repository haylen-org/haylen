#pragma once

#include <array>
#include <cstddef>

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A 3x3 matrix stored by columns, which multiplies a column vector on its right, such as the rotation and scale of a 3D transform or the matrix of its normals.
struct Mat3 {
    // The element of row `r` and column `c` sits at `values[c * 3 + r]`.
    std::array<float, 9> values{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    [[nodiscard]] static constexpr Mat3 identity() noexcept {
        return {};
    }
    [[nodiscard]] static constexpr Mat3 fromColumns(Vec3 x, Vec3 y, Vec3 z) noexcept {
        return {{x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z}};
    }
    [[nodiscard]] static constexpr Mat3 scaling(Vec3 scale) noexcept {
        return {{scale.x, 0.0F, 0.0F, 0.0F, scale.y, 0.0F, 0.0F, 0.0F, scale.z}};
    }

    [[nodiscard]] constexpr bool operator==(const Mat3&) const noexcept = default;

    [[nodiscard]] constexpr float get(std::size_t row, std::size_t column) const noexcept {
        return values[column * 3 + row];
    }
    [[nodiscard]] constexpr Vec3 getColumn(std::size_t column) const noexcept {
        return {values[column * 3], values[column * 3 + 1], values[column * 3 + 2]};
    }
    [[nodiscard]] constexpr Vec3 getRow(std::size_t row) const noexcept {
        return {values[row], values[3 + row], values[6 + row]};
    }

    [[nodiscard]] constexpr Mat3 operator*(const Mat3& rhs) const noexcept {
        Mat3 product;
        for (std::size_t column = 0; column < 3; ++column) {
            const Vec3 mapped = *this * rhs.getColumn(column);
            product.values[column * 3] = mapped.x;
            product.values[column * 3 + 1] = mapped.y;
            product.values[column * 3 + 2] = mapped.z;
        }

        return product;
    }

    [[nodiscard]] constexpr Vec3 operator*(Vec3 vector) const noexcept {
        return {values[0] * vector.x + values[3] * vector.y + values[6] * vector.z, values[1] * vector.x + values[4] * vector.y + values[7] * vector.z, values[2] * vector.x + values[5] * vector.y + values[8] * vector.z};
    }

    [[nodiscard]] constexpr Mat3 getTransposed() const noexcept {
        return fromColumns(getRow(0), getRow(1), getRow(2));
    }

    [[nodiscard]] constexpr float getDeterminant() const noexcept {
        return Vec3::dot(getColumn(0), Vec3::cross(getColumn(1), getColumn(2)));
    }

    // A matrix is invertible when its determinant stays clear of zero relative to the lengths of its columns, so a matrix that squashes space onto a plane or a line counts as singular even after rounding.
    [[nodiscard]] bool isInvertible() const noexcept;

    // Throws `std::invalid_argument` for a singular matrix, which has no inverse.
    [[nodiscard]] Mat3 getInverse() const;

    [[nodiscard]] static bool approximately(const Mat3& lhs, const Mat3& rhs, float epsilon = 1e-5F) noexcept;
};

} // namespace haylen::math
