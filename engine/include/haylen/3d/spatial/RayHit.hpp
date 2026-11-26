#pragma once

#include <cstdint>

#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// Where a ray meets the bounds of an entry of a spatial structure. Bounds are solid, so a ray that starts inside them hits at distance zero with a zero normal.
struct RayHit {
    std::uint64_t id = 0;
    math::Vec3 point;
    math::Vec3 normal;
    float distance = 0.0F;
};

} // namespace haylen::spatial3d
