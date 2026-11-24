#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A place on a curve with three directions at right angles: the tangent along the curve, the normal and the binormal, the tangent crossed with the normal, such as the frame that a tube, a rail or a camera on a track turns with.
struct CurveFrame {
    Vec3 position;
    Vec3 tangent;
    Vec3 normal;
    Vec3 binormal;
};

} // namespace haylen::math
