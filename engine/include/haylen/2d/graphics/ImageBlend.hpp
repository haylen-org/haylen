#pragma once

#include <cstdint>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Two images drawn over one area and mixed through a pattern that a progress from 0 to 1 advances, the building block of the shader-based scene transitions.
struct ImageBlend {
    enum class Pattern : std::uint8_t {
        // Cells switch from the first image to the second one in random order, with a soft edge.
        Dissolve,
        // The first image breaks into blocks that grow up to the block size, and the second image comes back out of them.
        Pixelate,
        // A hand sweeping around the center uncovers the second image, clockwise unless reversed.
        Radial,
        // A circle closing on the center covers the first image with the color, and a circle opening from it uncovers the second image.
        Iris,
        // The first image curls away like a page turned toward the angle and uncovers the second image.
        PageTurn,
    };

    Pattern pattern = Pattern::Dissolve;
    graphics::Texture from;
    graphics::Texture to;
    math::Rect area{};
    float progress = 0.0F;

    // Where the radial and iris patterns center, from (0, 0) at the top-left of the area to (1, 1) at its bottom-right.
    math::Vec2 center{0.5F, 0.5F};

    // Sizes in pixels of the first image: the dissolve cells and the largest pixelate blocks.
    float cellSize = 1.0F;
    float blockSize = 48.0F;
    math::Color color = math::Color::black();
    bool reversed = false;

    // Direction in radians the page turn moves toward, where 0 points right and a quarter turn points down.
    float angle = 0.0F;
};

} // namespace haylen::graphics2d
