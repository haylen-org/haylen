#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "haylen/2d/graphics/Glass.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// One glass as the frame draws it: its look, the index of its shape among the glass shapes of the frame, the corners in units of its canvas of what it reads, its shape with the reach of its refraction, the radius of its blur and the reach of its refraction in pixels of its destination, and the images of glass it renders through.
struct GlassDraw {
    Glass glass;
    std::uint32_t instance = 0;
    std::array<math::Vec2, 4> corners{};
    float blurPixels = 0.0F;
    float refractionPixels = 0.0F;
    std::size_t targets = 0;
};

} // namespace haylen::graphics2d
