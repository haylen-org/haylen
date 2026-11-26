#pragma once

#include "haylen/math/Vec3.hpp"

namespace haylen::animation3d {

// What skinning and morphing do to the vertices they move alike.
class VertexHelper3D final {
  public:
    // Returns a normal or a tangent with a length of one again, and keeps one that a joint scaled to zero or targets cancelled at zero, since it holds no direction.
    [[nodiscard]] static math::Vec3 unitOrZero(math::Vec3 vector) noexcept {
        const float length = vector.getLength();
        return length > 0.0F ? vector * (1.0F / length) : math::Vec3{};
    }
};

} // namespace haylen::animation3d
