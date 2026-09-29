#pragma once

#include "2d/lighting/ShadowMap.hpp"
#include "haylen/2d/lighting/Light.hpp"

namespace haylen::graphics2d {

// A light of a lit canvas as the light pass draws it, with the row of the shadow map it reads, or -1 without shadows, and where the texels of a directional map lie.
struct LightDraw {
    lighting2d::Light light;
    int shadowRow = -1;
    lighting2d::ShadowMap::Axis axis{};
};

} // namespace haylen::graphics2d
