#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Affine 2D transform that maps a point `p` to `(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty)`.
struct Transform2D {
    float a = 1.0F;
    float b = 0.0F;
    float c = 0.0F;
    float d = 1.0F;
    float tx = 0.0F;
    float ty = 0.0F;

    [[nodiscard]] static constexpr Transform2D identity() noexcept {
        return {};
    }
    [[nodiscard]] static constexpr Transform2D translation(Vec2 offset) noexcept {
        return {1.0F, 0.0F, 0.0F, 1.0F, offset.x, offset.y};
    }
    [[nodiscard]] static constexpr Transform2D scaling(Vec2 scale) noexcept {
        return {scale.x, 0.0F, 0.0F, scale.y, 0.0F, 0.0F};
    }
    [[nodiscard]] static Transform2D rotation(float radians) noexcept;
    [[nodiscard]] static Transform2D compose(Vec2 position, float angle, Vec2 scale, Vec2 skew = {}) noexcept;

    [[nodiscard]] constexpr Transform2D operator*(const Transform2D& rhs) const noexcept {
        return {
            a * rhs.a + c * rhs.b, b * rhs.a + d * rhs.b, a * rhs.c + c * rhs.d, b * rhs.c + d * rhs.d, a * rhs.tx + c * rhs.ty + tx, b * rhs.tx + d * rhs.ty + ty,
        };
    }

    [[nodiscard]] constexpr Vec2 apply(Vec2 point) const noexcept {
        return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
    }
    [[nodiscard]] constexpr Vec2 applyVector(Vec2 vector) const noexcept {
        return {a * vector.x + c * vector.y, b * vector.x + d * vector.y};
    }
    [[nodiscard]] constexpr float getDeterminant() const noexcept {
        return a * d - b * c;
    }
    [[nodiscard]] constexpr Vec2 getTranslation() const noexcept {
        return {tx, ty};
    }
    [[nodiscard]] Transform2D getInverse() const noexcept;
};

} // namespace haylen::math
