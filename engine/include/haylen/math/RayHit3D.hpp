#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// Where a 3D ray hits a shape: its distance from the origin of the ray, the point and the normal of the surface, which faces the ray.
struct RayHit3D {
    float distance = 0.0F;
    Vec3 point;
    Vec3 normal;
};

} // namespace haylen::math
