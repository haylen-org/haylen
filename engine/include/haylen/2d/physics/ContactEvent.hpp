#pragma once

#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

struct ContactEvent {
    Shape first;
    Shape second;
    math::Vec2 point{};
    math::Vec2 normal{};
    float speed = 0.0F;
};

} // namespace haylen::physics2d
