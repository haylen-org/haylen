#pragma once

#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// The colors of the parts of a sprite drawn with a part mask, named after the color that marks each part in the mask. A part takes its color multiplied by the shading of the sprite, mixed in by the alpha of the color, so white or a transparent color keeps the part as it is.
struct PartColors {
    math::Color red = math::Color::white();
    math::Color green = math::Color::white();
    math::Color blue = math::Color::white();
    math::Color yellow = math::Color::white();

    [[nodiscard]] constexpr bool operator==(const PartColors&) const noexcept = default;
};

} // namespace haylen::graphics2d
