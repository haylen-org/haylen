#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <stb_truetype.h>

namespace msdfgen {
class Shape;
}

namespace haylen::text {

// The signed distance field of one glyph of a TrueType or OpenType font, built from its whole outline, the cubic curves of CFF outlines included. Its bytes run in rows from the top, and the edge of the glyph lies halfway between 0 and 255 with the inside above it.
struct DistanceField {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;

    // Where the field starts from the pen on the baseline, in pixels.
    int offsetX = 0;
    int offsetY = 0;

    // Builds the field of a glyph at a scale in pixels per font unit, reaching the spread in pixels past the box of the glyph, or nothing for a glyph without an outline, such as a space.
    [[nodiscard]] static std::optional<DistanceField> build(const stbtt_fontinfo& font, int glyph, float scale, int spread);

  private:
    [[nodiscard]] static msdfgen::Shape readShape(const stbtt_fontinfo& font, int glyph, float scale);
    [[nodiscard]] static bool mayOverlap(const msdfgen::Shape& shape);
};

} // namespace haylen::text
