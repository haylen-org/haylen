#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Material.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// Full-screen adjustments applied when a world canvas is composited onto its destination. The materials run last, in order, each over the image the previous step made. Distances are in units of the destination of the canvas: design units for world canvases and pixels for render target canvases.
struct PostProcess {
    // The deficiencies of color vision that the image can show or correct for: red, green or blue cones that do not work.
    enum class ColorVision : std::uint8_t {
        Normal,
        Protanopia,
        Deuteranopia,
        Tritanopia,
    };

    static constexpr std::array<std::pair<std::string_view, ColorVision>, 4> kColorVisionNames{{
        {"normal", ColorVision::Normal},
        {"protanopia", ColorVision::Protanopia},
        {"deuteranopia", ColorVision::Deuteranopia},
        {"tritanopia", ColorVision::Tritanopia},
    }};

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

    // Shows the image as the deficiency of color vision sees it, to check that an app reads without the colors it loses, or with `correctColorVision` shifts the differences the deficiency cannot see into colors it can, for players who have it. It applies after the lookup texture.
    ColorVision colorVision = ColorVision::Normal;
    bool correctColorVision = false;

    // Adds noise that changes every frame, from 0 to 1, like the grain of film.
    float grain = 0.0F;

    // Spreads the rounding of every color to 8 bits over neighboring pixels, so smooth gradients, fog and light fall off without bands.
    bool dither = false;

    std::vector<Material> materials;

    // The rows of the matrix that turns a color into the one that the color vision option shows, which is the identity for normal vision.
    [[nodiscard]] std::array<std::array<float, 3>, 3> getColorVisionMatrix() const noexcept;
};

} // namespace haylen::graphics2d
