#pragma once

#include <array>
#include <vector>

#include "haylen/graphics/RenderTarget.hpp"

namespace haylen::graphics2d {

// The images of the glass draws over the destinations of one size: the image behind each glass with the size of the destination, which the glass reads at its own pixels, and the chain its blur shrinks into, a pair of targets at half the size and a pair at each smaller size the radius reaches.
struct GlassTargets {
    graphics::RenderTarget image;
    std::vector<std::array<graphics::RenderTarget, 2>> chain;
};

} // namespace haylen::graphics2d
