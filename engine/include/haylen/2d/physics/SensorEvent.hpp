#pragma once

#include "haylen/2d/physics/Shape.hpp"

namespace haylen::physics2d {

struct SensorEvent {
    Shape sensor;
    Shape visitor;
};

} // namespace haylen::physics2d
