#include "haylen/math/Transform2D.hpp"

#include <cmath>

namespace haylen::math {

Transform2D Transform2D::rotation(float radians) noexcept {
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return {cosine, sine, -sine, cosine, 0.0F, 0.0F};
}

Transform2D Transform2D::compose(Vec2 position, float angle, Vec2 scale, Vec2 skew) noexcept {
    const float sineX = std::sin(angle + skew.y);
    const float cosineX = std::cos(angle + skew.y);
    const float sineY = std::sin(angle + skew.x);
    const float cosineY = std::cos(angle + skew.x);
    return {cosineX * scale.x, sineX * scale.x, -sineY * scale.y, cosineY * scale.y, position.x, position.y};
}

Transform2D Transform2D::getInverse() const noexcept {
    const float det = getDeterminant();
    if (det == 0.0F) {
        return {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    }

    const float inverseDet = 1.0F / det;
    const float ia = d * inverseDet;
    const float ib = -b * inverseDet;
    const float ic = -c * inverseDet;
    const float id = a * inverseDet;
    return {ia, ib, ic, id, -(ia * tx + ic * ty), -(ib * tx + id * ty)};
}

} // namespace haylen::math
