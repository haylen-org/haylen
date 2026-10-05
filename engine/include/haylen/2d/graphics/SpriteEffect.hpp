#pragma once

#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// How a sprite dissolves into noise with a colored edge, and the outline and the glow around its visible pixels, which the effect program draws in one call for the sprites of a texture. The dissolve, its edge, the outline and the glow fade together as the dissolve grows, and sizes and widths are in pixels of the texture.
struct SpriteEffect {
    // How much of the sprite has dissolved, from 0 for none to 1 for all of it.
    float dissolve = 0.0F;

    // The share of the noise along the dissolving border that takes the dissolve color, from 0 to 1.
    float dissolveEdge = 0.08F;

    // The size of the cells of the dissolve noise, at least 1, where 1 dissolves pixel by pixel.
    float dissolveSize = 6.0F;
    math::Color dissolveColor = math::Color::transparent();
    float outlineWidth = 0.0F;
    math::Color outlineColor = math::Color::white();
    float glowSize = 0.0F;
    math::Color glowColor = math::Color::white();

    // The widest outline and glow, which the quad of the sprite grows by.
    static constexpr float kMaxReach = 64.0F;

    [[nodiscard]] bool isActive() const noexcept {
        return dissolve > 0.0F || outlineWidth > 0.0F || glowSize > 0.0F;
    }

    // Throws `std::invalid_argument` when a value is out of its range.
    void validate() const;
};

} // namespace haylen::graphics2d
