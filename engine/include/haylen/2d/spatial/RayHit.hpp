#pragma once

#include <cstdint>

#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Where a ray meets the bounds of an entry of a spatial structure. Bounds are solid, so a ray that starts inside them hits at distance zero with a zero normal.
struct RayHit {
    std::uint64_t id = 0;
    math::Vec2 point{};
    math::Vec2 normal{};
    float distance = 0.0F;
};

} // namespace haylen::spatial2d
