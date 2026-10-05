#pragma once

#include <cstdint>

#include "haylen/2d/graphics/Material.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {

// Placement and shading of a draw in the canvas. Higher layers draw later. Inside a layer, higher depths draw later when the canvas sorts by depth, and draws that stand lower draw later when it sorts by y. A draw whose visibility shares no bit with the visibility mask of its canvas is skipped. A material replaces how the draw shades its pixels.
struct DrawOrder {
    int layer = 0;
    float depth = 0.0F;

    // Moves the point a y-sorted canvas sorts the draw by, such as from the center of a sprite down to its feet.
    float sortOffset = 0.0F;
    std::uint32_t visibility = 1;
    graphics::BlendMode::Type blend = graphics::BlendMode::Type::Alpha;
    Material material;

    // Recolors sprites and sprite batches by parts: the mask shares the layout of the draw's texture and marks each part in red, green, blue or yellow, which take the part colors of each sprite. A recolored draw takes no material.
    graphics::Texture partMask;

    // How the draw takes light in a lit canvas. The normal map shares the layout of the draw's texture and lights sprites by the angle light reaches them at, the specular strength, scaled by the alpha of the normal map, and the shininess shape the highlights of normal-mapped sprites, the emission makes the colors glow whatever the light, lights reach the draw when its light mask shares a bit with their item mask, and unshaded draws keep their own colors.
    graphics::Texture normalMap;
    float specular = 0.0F;
    float shininess = 32.0F;
    float emission = 0.0F;
    std::uint8_t lightMask = 1;
    bool unshaded = false;
};

} // namespace haylen::graphics2d
