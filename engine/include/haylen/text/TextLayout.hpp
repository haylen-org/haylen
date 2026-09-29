#pragma once

#include <cstddef>
#include <vector>

#include "haylen/math/Vec2.hpp"
#include "haylen/text/GlyphQuad.hpp"

namespace haylen::text {

// The glyphs of a laid out text block, with the size of the block and its number of lines.
struct TextLayout {
    std::vector<GlyphQuad> quads;
    math::Vec2 size{};
    std::size_t lineCount = 0;
};

} // namespace haylen::text
