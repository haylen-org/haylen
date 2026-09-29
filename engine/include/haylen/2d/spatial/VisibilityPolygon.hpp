#pragma once

#include <span>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Computes the area visible from a point among wall segments, the shape of 2D lights and lines of sight. It keeps its buffers between calls, so computing a polygon every frame allocates nothing once the buffers have grown.
class VisibilityPolygon final {
  public:
    // Returns the outline of the area visible from the origin, clipped to the bounds, as points in order of increasing angle from -pi. The origin must lie inside the bounds. The points stay valid until the next call.
    const std::vector<math::Vec2>& compute(math::Vec2 origin, std::span<const math::Segment> walls, const math::Rect& bounds);

  private:
    // Rays pass this far on each side of every wall corner, so they reach past corners that do not block them.
    static constexpr float kCornerAngle = 1e-4F;

    std::vector<math::Segment> segments;
    std::vector<float> angles;
    std::vector<math::Vec2> points;
};

} // namespace haylen::spatial2d
