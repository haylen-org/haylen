#pragma once

#include <cstdint>
#include <memory>

#include <hb.h>

#include "graphics/VectorImageResource.hpp"

namespace haylen::text {

// The colors of one glyph of a color font as a vector image: the layers of its `COLR` table in the colors of the first `CPAL` palette, with their gradients, transforms and compositing, which the rasterizer of HarfBuzz paints. The image is the box of the glyph at the native size of the font, and the HarfBuzz font it holds keeps the font file alive while rasterizers on any thread read it.
class ColorGlyphImage final : public graphics::VectorImageResource {
  public:
    // The box is the extent of the glyph in font units, and the scale turns font units into pixels at the native size.
    ColorGlyphImage(hb_font_t& shapingFont, std::uint32_t glyphIndex, const hb_glyph_extents_t& box, float unitScale);

    // Paints the glyph so its box fills the raster. The parts that a font paints in the color of the text paint white, and a glyph the rasterizer cannot paint throws `std::runtime_error`.
    [[nodiscard]] graphics::Image rasterize(float scale) const override;

  private:
    [[nodiscard]] static std::uint8_t unpremultiply(unsigned int channel, unsigned int alpha) noexcept;

    std::unique_ptr<hb_font_t, decltype(&hb_font_destroy)> font;
    std::uint32_t glyph;
    hb_glyph_extents_t extents;
};

} // namespace haylen::text
