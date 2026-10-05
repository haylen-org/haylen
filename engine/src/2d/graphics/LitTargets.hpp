#pragma once

#include <array>

#include "haylen/graphics/RenderTarget.hpp"

namespace haylen::graphics2d {

// The offscreen targets of a composited canvas at the pixel size it covers. The scene takes the colors of its draws, and a lit canvas also fills the emission, the surface normals with their specular strength, and the light mask, layer and shininess the light pass reads into the light map. The distortion map adds up the coverage of distortion draws, the stages of the composite and the post-processing materials take turns between the two post targets, and bloom and blur work at half the size in their own pairs of targets.
struct LitTargets {
    graphics::RenderTarget scene;
    graphics::RenderTarget emission;
    graphics::RenderTarget surface;
    graphics::RenderTarget info;
    graphics::RenderTarget light;
    graphics::RenderTarget distortion;
    std::array<graphics::RenderTarget, 2> post;
    std::array<graphics::RenderTarget, 2> bloom;
    std::array<graphics::RenderTarget, 2> blur;
};

} // namespace haylen::graphics2d
