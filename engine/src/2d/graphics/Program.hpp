#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// The shader program that draws a command, composites a lit or post-processed canvas, draws a light into a light map, draws the surface of metaballs, recolors sprites by the parts of a mask, shrinks, blurs and copies images, draws sprites with an effect, draws shapes with exact edges, draws the sprites of a batch with several textures or sprite data, which read a second record of their own, or blends the image of a group with a copy of what lies below it.
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
    Backdrop,
};

} // namespace haylen::graphics2d
