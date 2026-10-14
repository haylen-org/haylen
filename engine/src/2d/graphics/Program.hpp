#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// The shader program that draws a command, composites a lit or post-processed canvas, draws a light into a light map, draws the surface of metaballs, recolors sprites by the parts of a mask, shrinks and blurs images for post-processing, draws sprites with an effect, draws shapes with exact edges or draws the sprites of a batch with several textures or sprite data, which read a second record of their own.
enum class Program : std::uint8_t {
    Sprite,
    Text,
    Mesh,
    ImageBlend,
    Composite,
    Light,
    Metaball,
    Recolor,
    Filter,
    Effect,
    Shape,
    Batch,
};

} // namespace haylen::graphics2d
