#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A point in cylindrical coordinates around the up axis: its `radius` from the axis, its `azimuth` around the axis by the right hand, 0 toward positive z, and its `height` along the axis, such as a place on a tower, a spiral or a carousel.
struct Cylindrical {
    float radius = 0.0F;
    float azimuth = 0.0F;
    float height = 0.0F;

    // Returns the coordinates of a point. A point on the axis has an azimuth of 0, and the azimuth runs from minus pi to pi.
    [[nodiscard]] static Cylindrical fromVector(Vec3 point) noexcept;

    [[nodiscard]] Vec3 toVector() const noexcept;
};

} // namespace haylen::math
