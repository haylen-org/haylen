#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <hb.h>

namespace msdfgen {
class Contour;
class Shape;
} // namespace msdfgen

namespace haylen::text {

class FontFile;

// The signed distance field of one glyph of a TrueType or OpenType font, built from its whole outline, the cubic curves of CFF outlines included. Its distances are half floats from 0 to 1 that run in rows from the top, with the edge of the glyph at 0.5 and the inside above it, so every GPU filters them without the steps of a byte.
struct DistanceField {
    std::vector<std::uint16_t> distances;
    int width = 0;
    int height = 0;

    // Where the field starts from the pen on the baseline, in pixels.
    int offsetX = 0;
    int offsetY = 0;

    // Builds the field of a glyph at a scale in pixels per font unit, reaching the spread in pixels past the box of the glyph, or nothing for a glyph without an outline, such as a space. A field wider or taller than `largest` pixels throws `std::runtime_error` before its distances are computed, which takes long at such sizes.
    [[nodiscard]] static std::optional<DistanceField> build(const FontFile& font, std::uint32_t glyph, float scale, int spread, int largest);

    // Returns the size and offset of the field `build` makes, without its pixels, or nothing for a glyph without an outline box. It reads only the box of the glyph, so it tells at once whether a field can fit somewhere.
    [[nodiscard]] static std::optional<DistanceField> measure(const FontFile& font, std::uint32_t glyph, float scale, int spread);

    // Returns twice the distance from the middle of the box of a glyph to its outline in pixels at the scale, the width of the hole that the middle lies in, such as the counter of the letter o, or 0 when the middle lies inside a stroke or the glyph has no outline.
    [[nodiscard]] static float measureHole(const FontFile& font, std::uint32_t glyph, float scale);

  private:
    // How many straight steps follow each edge when the area of an outline is summed.
    static constexpr int kAreaSteps = 8;

    // The outline that HarfBuzz draws, gathered in pixels with y pointing down like the rows of the field.
    struct Outline {
        msdfgen::Shape& shape;
        msdfgen::Contour* contour = nullptr;
        double penX = 0.0;
        double penY = 0.0;
        float scale = 1.0F;
    };

    [[nodiscard]] static hb_draw_funcs_t* getOutlineFuncs();
    [[nodiscard]] static hb_draw_funcs_t* makeOutlineFuncs();
    static void moveTo(hb_draw_funcs_t* funcs, void* outline, hb_draw_state_t* state, float toX, float toY, void* user);
    static void lineTo(hb_draw_funcs_t* funcs, void* outline, hb_draw_state_t* state, float toX, float toY, void* user);
    static void quadraticTo(hb_draw_funcs_t* funcs, void* outline, hb_draw_state_t* state, float controlX, float controlY, float toX, float toY, void* user);
    static void cubicTo(hb_draw_funcs_t* funcs, void* outline, hb_draw_state_t* state, float firstX, float firstY, float secondX, float secondY, float toX, float toY, void* user);

    [[nodiscard]] static msdfgen::Shape readShape(const FontFile& font, std::uint32_t glyph, float scale);
    [[nodiscard]] static bool mayOverlap(const msdfgen::Shape& shape);
    [[nodiscard]] static bool fillsBackward(const msdfgen::Shape& shape);
};

} // namespace haylen::text
