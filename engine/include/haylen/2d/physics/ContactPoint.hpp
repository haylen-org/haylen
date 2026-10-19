#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// One point where two shapes touch, in world units, as the last step solved it.
struct ContactPoint {
    math::Vec2 point{};

    // The distance between the surfaces at the point, negative while they overlap.
    float separation = 0.0F;

    // The impulses that pushed the shapes apart along the normal and held them along the surface in the last sub-step of the step, in kilograms times units per second.
    float normalImpulse = 0.0F;
    float tangentImpulse = 0.0F;

    // How fast the shapes closed in along the normal at the point before the step solved them, in units per second, and 0 when they were moving apart.
    float approachSpeed = 0.0F;
};

} // namespace haylen::physics2d
