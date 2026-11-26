#pragma once

#include <array>

#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

// Moves points between the vectors of the engine and the arrays of three floats that Recast and Detour take, which share the axes and the meters of the engine.
class DetourVector final {
  public:
    [[nodiscard]] static std::array<float, 3> of(math::Vec3 point) noexcept {
        return {point.x, point.y, point.z};
    }
    [[nodiscard]] static math::Vec3 toVec3(const float* point) noexcept {
        return {point[0], point[1], point[2]};
    }
};

} // namespace haylen::navigation3d
