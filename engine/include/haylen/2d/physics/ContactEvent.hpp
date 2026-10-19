#pragma once

#include "haylen/2d/physics/ContactManifold.hpp"
#include "haylen/2d/physics/Shape.hpp"

namespace haylen::physics2d {

// A contact that began, ended or hit in the last step between two shapes, with what it did. An ended contact keeps only its shapes.
struct ContactEvent {
    Shape first;
    Shape second;
    ContactManifold manifold;
};

} // namespace haylen::physics2d
