#include "haylen/math/Cylindrical.hpp"

#include <cmath>

namespace haylen::math {

Cylindrical Cylindrical::fromVector(Vec3 point) noexcept {
    const bool onAxis = point.x == 0.0F && point.z == 0.0F;
    return {.radius = std::hypot(point.x, point.z), .azimuth = onAxis ? 0.0F : std::atan2(point.x, point.z), .height = point.y};
}

Vec3 Cylindrical::toVector() const noexcept {
    return {radius * std::sin(azimuth), height, radius * std::cos(azimuth)};
}

} // namespace haylen::math
