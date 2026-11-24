#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/math/Math.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A 4D vector, such as a point in homogeneous coordinates with `w` of one or a direction with `w` of zero.
struct Vec4 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;

    [[nodiscard]] static constexpr Vec4 fromVec3(Vec3 value, float homogeneous) noexcept {
        return {value.x, value.y, value.z, homogeneous};
    }

    [[nodiscard]] constexpr Vec4 operator-() const noexcept {
        return {-x, -y, -z, -w};
    }
    [[nodiscard]] constexpr Vec4 operator+(Vec4 other) const noexcept {
        return {x + other.x, y + other.y, z + other.z, w + other.w};
    }
    [[nodiscard]] constexpr Vec4 operator-(Vec4 other) const noexcept {
        return {x - other.x, y - other.y, z - other.z, w - other.w};
    }
    [[nodiscard]] constexpr Vec4 operator*(Vec4 other) const noexcept {
        return {x * other.x, y * other.y, z * other.z, w * other.w};
    }
    [[nodiscard]] constexpr Vec4 operator/(Vec4 other) const noexcept {
        return {x / other.x, y / other.y, z / other.z, w / other.w};
    }
    [[nodiscard]] constexpr Vec4 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }
    [[nodiscard]] constexpr Vec4 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar, w / scalar};
    }
    [[nodiscard]] constexpr bool operator==(const Vec4&) const noexcept = default;

    [[nodiscard]] friend constexpr Vec4 operator*(float scalar, Vec4 value) noexcept {
        return value * scalar;
    }

    constexpr Vec4& operator+=(Vec4 other) noexcept {
        return *this = *this + other;
    }
    constexpr Vec4& operator-=(Vec4 other) noexcept {
        return *this = *this - other;
    }
    constexpr Vec4& operator*=(Vec4 other) noexcept {
        return *this = *this * other;
    }
    constexpr Vec4& operator*=(float scalar) noexcept {
        return *this = *this * scalar;
    }
    constexpr Vec4& operator/=(float scalar) noexcept {
        return *this = *this / scalar;
    }

    [[nodiscard]] constexpr Vec3 getXyz() const noexcept {
        return {x, y, z};
    }
    [[nodiscard]] constexpr float getLengthSquared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }
    [[nodiscard]] float getLength() const noexcept {
        return std::sqrt(getLengthSquared());
    }
    [[nodiscard]] constexpr bool isZero() const noexcept {
        return x == 0.0F && y == 0.0F && z == 0.0F && w == 0.0F;
    }
    [[nodiscard]] bool isFinite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }

    // Returns the vector with a length of one, scaled by its largest component first like `Vec3::getNormalized`. Throws `std::invalid_argument` for a zero vector or one with a component that is not finite.
    [[nodiscard]] Vec4 getNormalized() const {
        if (!isFinite()) {
            throw std::invalid_argument("The vector has no direction, because one of its components is not a finite number.");
        }

        const float largest = std::max({std::fabs(x), std::fabs(y), std::fabs(z), std::fabs(w)});
        if (largest == 0.0F) {
            throw std::invalid_argument("The vector has no direction, because its length is zero.");
        }

        const Vec4 scaled = *this / largest;
        return scaled / scaled.getLength();
    }

    [[nodiscard]] static constexpr float dot(Vec4 lhs, Vec4 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }

    [[nodiscard]] static constexpr float distanceSquared(Vec4 lhs, Vec4 rhs) noexcept {
        return (lhs - rhs).getLengthSquared();
    }

    [[nodiscard]] static float distance(Vec4 lhs, Vec4 rhs) noexcept {
        return (lhs - rhs).getLength();
    }

    // Returns `from` and `to` exactly at 0 and 1.
    [[nodiscard]] static constexpr Vec4 lerp(Vec4 from, Vec4 to, float t) noexcept {
        return from * (1.0F - t) + to * t;
    }

    [[nodiscard]] static constexpr Vec4 min(Vec4 lhs, Vec4 rhs) noexcept {
        return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z), std::min(lhs.w, rhs.w)};
    }

    [[nodiscard]] static constexpr Vec4 max(Vec4 lhs, Vec4 rhs) noexcept {
        return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z), std::max(lhs.w, rhs.w)};
    }

    [[nodiscard]] static bool approximately(Vec4 lhs, Vec4 rhs, float epsilon = 1e-5F) noexcept {
        return Math::approximately(lhs.x, rhs.x, epsilon) && Math::approximately(lhs.y, rhs.y, epsilon) && Math::approximately(lhs.z, rhs.z, epsilon) && Math::approximately(lhs.w, rhs.w, epsilon);
    }
};

} // namespace haylen::math
