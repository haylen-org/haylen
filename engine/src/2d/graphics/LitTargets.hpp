#pragma once

#include <array>

#include "haylen/graphics/RenderTarget.hpp"

namespace haylen::graphics2d {

// The offscreen targets of a composited canvas at the pixel size it covers. The scene takes the colors of its draws, and a lit canvas also fills the emission, the surface normals with their specular strength, and the light mask, layer and shininess the light pass reads into the light map. Post-processing materials take turns between the two post targets.
struct LitTargets {
    graphics::RenderTarget scene;
    graphics::RenderTarget emission;
    graphics::RenderTarget surface;
    graphics::RenderTarget info;
    graphics::RenderTarget light;
    std::array<graphics::RenderTarget, 2> post;
};

} // namespace haylen::graphics2d
