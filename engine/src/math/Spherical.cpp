#include "haylen/math/Spherical.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::math {

Spherical Spherical::fromVector(Vec3 point) noexcept {
    const float radius = point.getLength();
    if (radius == 0.0F) {
        return {};
    }

    const float polar = std::acos(std::clamp(point.y / radius, -1.0F, 1.0F));
    const bool onAxis = point.x == 0.0F && point.z == 0.0F;
    return {.radius = radius, .polar = polar, .azimuth = onAxis ? 0.0F : std::atan2(point.x, point.z)};
}

Vec3 Spherical::toVector() const noexcept {
    const float across = radius * std::sin(polar);
    return {across * std::sin(azimuth), radius * std::cos(polar), across * std::cos(azimuth)};
}

} // namespace haylen::math
