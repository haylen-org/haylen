#pragma once

#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// Where a ray or a swept shape meets a shape of the world. The fraction runs from 0 at the start of the cast to 1 at its end, and the distance measures the same span in world units. Shape casts that start overlapping a shape hit it at fraction 0 with a zero normal.
struct RaycastHit {
    Shape shape;
    math::Vec2 point{};
    math::Vec2 normal{};
    float fraction = 0.0F;
    float distance = 0.0F;
};

} // namespace haylen::physics2d
