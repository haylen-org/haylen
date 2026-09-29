#pragma once

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/TextAlign.hpp"

namespace haylen::text {

// How a block of text looks and where it sits. A positive maximum width wraps whole words, and the anchor is a fraction of the block size that lands on the draw position. Outlines and blurred shadows need a font with a distance field.
struct TextStyle {
    float size = 32.0F;
    math::Color color = math::Color::white();
    float outlineWidth = 0.0F;
    math::Color outlineColor = math::Color::black();
    math::Vec2 shadowOffset{};
    math::Color shadowColor = math::Color::transparent();
    float shadowBlur = 0.0F;
    TextAlign align = TextAlign::Left;
    float maxWidth = 0.0F;
    float lineSpacing = 1.2F;
    math::Vec2 anchor{};
    float rotation = 0.0F;
};

} // namespace haylen::text
