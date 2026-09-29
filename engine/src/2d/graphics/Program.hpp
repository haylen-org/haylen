#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// The shader program that draws a command, composites a lit or post-processed canvas, draws a light into a light map or draws the surface of metaballs.
enum class Program : std::uint8_t {
    Sprite,
    Text,
    Mesh,
    ImageBlend,
    Composite,
    Light,
    Metaball,
};

} // namespace haylen::graphics2d
