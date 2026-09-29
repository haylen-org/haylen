#pragma once

#include <string>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/TextAlign.hpp"

namespace haylen::text {

// How a block of text looks and where it sits. A positive maximum width wraps lines where the Unicode line breaking rules allow, and the anchor is a fraction of the block size that lands on the draw position. Outlines and blurred shadows need a font with a distance field. Bold and italic pick those faces of a family, synthesized when it lacks them. The direction reads every paragraph left to right, right to left, or from its first strong letter, and the language, a BCP 47 tag such as ar, hi or ja, picks the letter forms and line breaks of its script.
struct TextStyle {
    float size = 32.0F;
    math::Color color = math::Color::white();
    float outlineWidth = 0.0F;
    math::Color outlineColor = math::Color::black();
    math::Vec2 shadowOffset{};
    math::Color shadowColor = math::Color::transparent();
    float shadowBlur = 0.0F;
    TextAlign align = TextAlign::Start;
    float maxWidth = 0.0F;
    float lineSpacing = 1.2F;
    math::Vec2 anchor{};
    float rotation = 0.0F;
    bool bold = false;
    bool italic = false;
    Direction direction = Direction::Auto;
    std::string language;
};

} // namespace haylen::text
