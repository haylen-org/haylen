#pragma once

#include <cstdint>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::text {

// One laid out glyph: where it goes in the text block, how large it is, which page of the font it comes from and which region of that page it shows.
struct GlyphQuad {
    math::Vec2 position{};
    math::Vec2 size{};
    math::Rect source{};
    std::uint16_t page = 0;
};

} // namespace haylen::text
