#pragma once

#include <cmath>

namespace haylen::math {

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;

    [[nodiscard]] constexpr Vec2 operator-() const noexcept {
        return {-x, -y};
    }
    [[nodiscard]] constexpr Vec2 operator+(Vec2 other) const noexcept {
        return {x + other.x, y + other.y};
    }
    [[nodiscard]] constexpr Vec2 operator-(Vec2 other) const noexcept {
        return {x - other.x, y - other.y};
    }
    [[nodiscard]] constexpr Vec2 operator*(Vec2 other) const noexcept {
        return {x * other.x, y * other.y};
    }
    [[nodiscard]] constexpr Vec2 operator/(Vec2 other) const noexcept {
        return {x / other.x, y / other.y};
    }
    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }
    [[nodiscard]] constexpr Vec2 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar};
    }
    [[nodiscard]] constexpr bool operator==(const Vec2&) const noexcept = default;

    [[nodiscard]] friend constexpr Vec2 operator*(float scalar, Vec2 value) noexcept {
        return value * scalar;
    }

    constexpr Vec2& operator+=(Vec2 other) noexcept {
        return *this = *this + other;
    }
    constexpr Vec2& operator-=(Vec2 other) noexcept {
        return *this = *this - other;
    }
    constexpr Vec2& operator*=(Vec2 other) noexcept {
        return *this = *this * other;
    }
    constexpr Vec2& operator*=(float scalar) noexcept {
        return *this = *this * scalar;
    }
    constexpr Vec2& operator/=(float scalar) noexcept {
        return *this = *this / scalar;
    }

    [[nodiscard]] constexpr float getLengthSquared() const noexcept {
        return x * x + y * y;
    }
    [[nodiscard]] float getLength() const noexcept {
        return std::sqrt(getLengthSquared());
    }
    [[nodiscard]] float getAngle() const noexcept {
        return std::atan2(y, x);
    }
    [[nodiscard]] constexpr bool isZero() const noexcept {
        return x == 0.0F && y == 0.0F;
    }

    [[nodiscard]] Vec2 getNormalized() const noexcept {
        const float magnitude = getLength();
        if (magnitude <= 1e-6F) {
            return {};
        }
        return *this / magnitude;
    }

    [[nodiscard]] constexpr Vec2 getPerpendicular() const noexcept {
        return {-y, x};
    }

    [[nodiscard]] Vec2 rotated(float radians) const noexcept {
        const float sine = std::sin(radians);
        const float cosine = std::cos(radians);
        return {x * cosine - y * sine, x * sine + y * cosine};
    }

    [[nodiscard]] Vec2 clampedLength(float maximum) const noexcept {
        const float squared = getLengthSquared();
        if (squared <= maximum * maximum) {
            return *this;
        }
        return *this * (maximum / std::sqrt(squared));
    }

    [[nodiscard]] static Vec2 fromAngle(float radians, float magnitude = 1.0F) noexcept {
        return {std::cos(radians) * magnitude, std::sin(radians) * magnitude};
    }

    [[nodiscard]] static constexpr float dot(Vec2 lhs, Vec2 rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y;
    }

    [[nodiscard]] static constexpr float cross(Vec2 lhs, Vec2 rhs) noexcept {
        return lhs.x * rhs.y - lhs.y * rhs.x;
    }

    [[nodiscard]] static constexpr float distanceSquared(Vec2 lhs, Vec2 rhs) noexcept {
        return (lhs - rhs).getLengthSquared();
    }

    [[nodiscard]] static float distance(Vec2 lhs, Vec2 rhs) noexcept {
        return (lhs - rhs).getLength();
    }

    [[nodiscard]] static constexpr Vec2 lerp(Vec2 from, Vec2 to, float t) noexcept {
        return from + (to - from) * t;
    }

    [[nodiscard]] static constexpr Vec2 min(Vec2 lhs, Vec2 rhs) noexcept {
        return {lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y};
    }

    [[nodiscard]] static constexpr Vec2 max(Vec2 lhs, Vec2 rhs) noexcept {
        return {lhs.x > rhs.x ? lhs.x : rhs.x, lhs.y > rhs.y ? lhs.y : rhs.y};
    }

    [[nodiscard]] static Vec2 floor(Vec2 value) noexcept {
        return {std::floor(value.x), std::floor(value.y)};
    }

    [[nodiscard]] static Vec2 round(Vec2 value) noexcept {
        return {std::round(value.x), std::round(value.y)};
    }
};

} // namespace haylen::math
