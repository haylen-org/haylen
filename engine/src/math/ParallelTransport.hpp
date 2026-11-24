#pragma once

#include <span>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// Carries a normal along the frames of a curve by parallel transport, the double reflection of Wang and others, so the frames never twist around the tangent more than the curve does.
class ParallelTransport final {
  public:
    // Fills the normals and binormals of frames whose positions and unit tangents are set. The first normal is `up` made perpendicular to the first tangent, or the axis of the world most across the tangent when `up` runs along it. A closed curve spreads the twist that its loop gathers over all its frames, so the last frame meets the first. Throws `std::invalid_argument` for an `up` without a direction.
    static void carry(std::span<CurveFrame> frames, Vec3 up, bool closed);
};

} // namespace haylen::math
