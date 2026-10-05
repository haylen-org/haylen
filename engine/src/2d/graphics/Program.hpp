#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// The shader program that draws a command, composites a lit or post-processed canvas, draws a light into a light map, draws the surface of metaballs, recolors sprites by the parts of a mask, shrinks and blurs images for post-processing, draws sprites with an effect or draws shapes with exact edges.
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
};

} // namespace haylen::graphics2d
