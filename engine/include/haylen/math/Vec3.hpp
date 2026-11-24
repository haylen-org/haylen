#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

// A 3D vector in the right-handed space of the engine, where y points up, x points right and a camera looks along negative z.
struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] static constexpr Vec3 right() noexcept {
        return {1.0F, 0.0F, 0.0F};
    }
    [[nodiscard]] static constexpr Vec3 up() noexcept {
        return {0.0F, 1.0F, 0.0F};
    }
    [[nodiscard]] static constexpr Vec3 forward() noexcept {
        return {0.0F, 0.0F, -1.0F};
    }

    [[nodiscard]] constexpr Vec3 operator-() const noexcept {
        return {-x, -y, -z};
    }
    [[nodiscard]] constexpr Vec3 operator+(Vec3 other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    [[nodiscard]] constexpr Vec3 operator-(Vec3 other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    [[nodiscard]] constexpr Vec3 operator*(Vec3 other) const noexcept {
        return {x * other.x, y * other.y, z * other.z};
    }
    [[nodiscard]] constexpr Vec3 operator/(Vec3 other) const noexcept {
        return {x / other.x, y / other.y, z / other.z};
    }
    [[nodiscard]] constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    [[nodiscard]] constexpr Vec3 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar};
    }
    [[nodiscard]] constexpr bool operator==(const Vec3&) const noexcept = default;

    [[nodiscard]] friend constexpr Vec3 operator*(float scalar, Vec3 value) noexcept {
        return value * scalar;
    }

    constexpr Vec3& operator+=(Vec3 other) noexcept {
        return *this = *this + other;
    }
    constexpr Vec3& operator-=(Vec3 other) noexcept {
        return *this = *this - other;
    }
    constexpr Vec3& operator*=(Vec3 other) noexcept {
        return *this = *this * other;
    }
    constexpr Vec3& operator*=(float scalar) noexcept {
        return *this = *this * scalar;
    }
    constexpr Vec3& operator/=(float scalar) noexcept {
        return *this = *this / scalar;
    }

    [[nodiscard]] constexpr float getLengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] float getLength() const noexcept {
        return std::sqrt(getLengthSquared());
    }
    [[nodiscard]] constexpr bool isZero() const noexcept {
        return x == 0.0F && y == 0.0F && z == 0.0F;
    }
    [[nodiscard]] bool isFinite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    // Returns the vector with a length of one. It scales by the largest component first, so vectors far below or above the range of a squared float keep their direction. Throws `std::invalid_argument` for a zero vector or one with a component that is not finite, which have no direction.
    [[nodiscard]] Vec3 getNormalized() const {
        if (!isFinite()) {
            throw std::invalid_argument("The vector has no direction, because one of its components is not a finite number.");
        }

        const float largest = std::max({std::fabs(x), std::fabs(y), std::fabs(z)});
        if (largest == 0.0F) {
            throw std::invalid_argument("The vector has no direction, because its length is zero.");
        }

        const Vec3 scaled = *this / largest;
        return scaled / scaled.getLength();
    }

    [[nodiscard]] static constexpr float dot(Vec3 lhs, Vec3 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    // Returns the vector perpendicular to both, which follows the right hand: the cross product of right and up is the opposite of forward.
    [[nodiscard]] static constexpr Vec3 cross(Vec3 lhs, Vec3 rhs) noexcept {
        return {lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x};
    }

    [[nodiscard]] static constexpr float distanceSquared(Vec3 lhs, Vec3 rhs) noexcept {
        return (lhs - rhs).getLengthSquared();
    }

    [[nodiscard]] static float distance(Vec3 lhs, Vec3 rhs) noexcept {
        return (lhs - rhs).getLength();
    }

    // Returns the part of `vector` that runs along `onto`. Throws `std::invalid_argument` when `onto` is a zero vector, which has no direction.
    [[nodiscard]] static Vec3 project(Vec3 vector, Vec3 onto) {
        const float squared = onto.getLengthSquared();
        if (squared == 0.0F || !std::isfinite(squared)) {
            throw std::invalid_argument("A vector projects only onto a vector with a direction, and this one has a zero or infinite length.");
        }

        return onto * (dot(vector, onto) / squared);
    }

    // Returns the part of `vector` that lies in the plane through the origin with the given normal, such as a velocity that slides along a wall.
    [[nodiscard]] static Vec3 projectOnPlane(Vec3 vector, Vec3 normal) {
        return vector - project(vector, normal);
    }

    // Returns the direction mirrored by the surface whose normal has a length of one.
    [[nodiscard]] static constexpr Vec3 reflect(Vec3 direction, Vec3 normal) noexcept {
        return direction - normal * (2.0F * dot(direction, normal));
    }

    // Returns the direction that bends through a surface whose normal has a length of one and faces the incoming direction, for a unit direction and the ratio of the refractive indices of both sides, or nothing when the light reflects totally inside.
    [[nodiscard]] static std::optional<Vec3> refract(Vec3 direction, Vec3 normal, float ratio) noexcept {
        const float cosine = -dot(direction, normal);
        const float sineSquared = ratio * ratio * (1.0F - cosine * cosine);
        if (sineSquared > 1.0F) {
            return std::nullopt;
        }

        return direction * ratio + normal * (ratio * cosine - std::sqrt(1.0F - sineSquared));
    }

    // Returns `from` and `to` exactly at 0 and 1.
    [[nodiscard]] static constexpr Vec3 lerp(Vec3 from, Vec3 to, float t) noexcept {
        return from * (1.0F - t) + to * t;
    }

    [[nodiscard]] static constexpr Vec3 min(Vec3 lhs, Vec3 rhs) noexcept {
        return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y), std::min(lhs.z, rhs.z)};
    }

    [[nodiscard]] static constexpr Vec3 max(Vec3 lhs, Vec3 rhs) noexcept {
        return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y), std::max(lhs.z, rhs.z)};
    }

    // Compares every component with the relative tolerance of `Math::approximately`, so NaN never equals anything.
    [[nodiscard]] static bool approximately(Vec3 lhs, Vec3 rhs, float epsilon = 1e-5F) noexcept {
        return Math::approximately(lhs.x, rhs.x, epsilon) && Math::approximately(lhs.y, rhs.y, epsilon) && Math::approximately(lhs.z, rhs.z, epsilon);
    }
};

} // namespace haylen::math
