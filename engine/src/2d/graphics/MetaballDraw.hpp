#pragma once

#include <cstddef>
#include <cstdint>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// One `drawMetaballs` call: the soft circles it splats into its field target, the world area its surface can cover and how the surface shows.
struct MetaballDraw {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    math::Rect area{};
    Renderer::MetaballStyle style{};
    std::size_t field = 0;
};

} // namespace haylen::graphics2d
