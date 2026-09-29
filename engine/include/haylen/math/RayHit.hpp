#pragma once

#include <cstddef>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Where a ray meets a surface. The normal is a unit vector that faces the side the ray came from, or zero when the ray starts inside a solid shape. Casts against lists report the index of the element or edge they hit.
struct RayHit {
    Vec2 point{};
    Vec2 normal{};
    float distance = 0.0F;
    std::size_t index = 0;
};

} // namespace haylen::math
