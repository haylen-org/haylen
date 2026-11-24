#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A point in spherical coordinates around the origin: its `radius`, its `polar` angle down from the up axis, from 0 at the top to pi at the bottom, and its `azimuth` around the up axis by the right hand, 0 toward positive z, such as the place of a camera that orbits a target or a direction on the sky.
struct Spherical {
    float radius = 0.0F;
    float polar = 0.0F;
    float azimuth = 0.0F;

    // Returns the coordinates of a point. A point on the up axis has an azimuth of 0, and the origin also a polar angle of 0. The azimuth runs from minus pi to pi.
    [[nodiscard]] static Spherical fromVector(Vec3 point) noexcept;

    [[nodiscard]] Vec3 toVector() const noexcept;
};

} // namespace haylen::math
