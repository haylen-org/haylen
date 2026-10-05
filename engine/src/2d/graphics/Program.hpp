#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// The shader program that draws a command, composites a lit or post-processed canvas, draws a light into a light map, draws the surface of metaballs or recolors sprites by the parts of a mask.
enum class Program : std::uint8_t {
    Sprite,
    Text,
    Mesh,
    ImageBlend,
    Composite,
    Light,
    Metaball,
    Recolor,
};

} // namespace haylen::graphics2d
