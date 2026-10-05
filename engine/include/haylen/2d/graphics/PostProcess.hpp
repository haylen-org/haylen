#pragma once

#include <vector>

#include "haylen/2d/graphics/Material.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// Full-screen adjustments applied when a world canvas is composited onto its destination. The materials run last, in order, each over the image the previous step made. Distances are in units of the destination of the canvas: design units for world canvases and pixels for render target canvases.
struct PostProcess {
    math::Color tint = math::Color::white();
    float saturation = 1.0F;
    float brightness = 1.0F;
    float contrast = 1.0F;
    float vignetteStrength = 0.0F;
    float vignetteRadius = 0.6F;
    float vignetteSoftness = 0.5F;
    math::Color fade = math::Color::transparent();

    // How far the draws with a distortion move pixels where their coverage goes from nothing to full within 8 units.
    float distortion = 24.0F;

    // Splits the colors toward the corners by this distance, like a cheap lens.
    float chromaticAberration = 0.0F;

    // Draws the image in square blocks of this size, where 0 keeps every pixel.
    float pixelate = 0.0F;

    // Blurs the whole image over this radius, such as the world behind a pause menu.
    float blur = 0.0F;

    // Adds the parts of the image brighter than the threshold, blurred over the radius, times the strength, so lights and fire glow.
    float bloomStrength = 0.0F;
    float bloomThreshold = 0.8F;
    float bloomRadius = 12.0F;

    // Grades the colors through a lookup texture of square cells side by side, one cell for each step of blue, with red across a cell and green down it, mixed in by the strength. The texture of an identity grading of 16 steps is 256 by 16 pixels.
    graphics::Texture colorLut;
    float colorLutStrength = 1.0F;

    std::vector<Material> materials;
};

} // namespace haylen::graphics2d
