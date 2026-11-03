#include "text/ColorGlyphImage.hpp"

#include <algorithm>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <hb-raster.h>

#include "haylen/graphics/VectorImage.hpp"

namespace haylen::text {

ColorGlyphImage::ColorGlyphImage(hb_font_t& shapingFont, std::uint32_t glyphIndex, const hb_glyph_extents_t& box, float unitScale) : VectorImageResource({static_cast<float>(box.width) * unitScale, static_cast<float>(-box.height) * unitScale}), font(hb_font_reference(&shapingFont), &hb_font_destroy), glyph(glyphIndex), extents(box) {}

std::uint8_t ColorGlyphImage::unpremultiply(unsigned int channel, unsigned int alpha) noexcept {
    return static_cast<std::uint8_t>(std::min(255U, (channel * 255U + alpha / 2U) / alpha));
}

graphics::Image ColorGlyphImage::rasterize(float scale) const {
    const auto [width, height] = graphics::VectorImage::getRasterSize(getSize(), scale);
    const std::unique_ptr<hb_raster_paint_t, decltype(&hb_raster_paint_destroy)> paint(hb_raster_paint_create_or_fail(), &hb_raster_paint_destroy);
    if (!paint) {
        throw std::bad_alloc();
    }

    // The box of the glyph, whose y axis points up in font units, fills the raster, which HarfBuzz stores from its bottom row up.
    const float scaleX = static_cast<float>(width) / static_cast<float>(extents.width);
    const float scaleY = static_cast<float>(height) / static_cast<float>(-extents.height);
    const float bottom = static_cast<float>(extents.y_bearing + extents.height);
    hb_raster_paint_set_transform(paint.get(), scaleX, 0.0F, 0.0F, scaleY, -static_cast<float>(extents.x_bearing) * scaleX, -bottom * scaleY);
    const hb_raster_extents_t area{.x_origin = 0, .y_origin = 0, .width = static_cast<unsigned int>(width), .height = static_cast<unsigned int>(height), .stride = static_cast<unsigned int>(width) * 4U};
    hb_raster_paint_set_extents(paint.get(), &area);
    hb_raster_paint_set_foreground(paint.get(), HB_COLOR(255, 255, 255, 255));
    if (!hb_raster_paint_glyph_or_fail(paint.get(), font.get(), glyph)) {
        throw std::runtime_error("The color glyph " + std::to_string(glyph) + " of the font could not be painted.");
    }
    const std::unique_ptr<hb_raster_image_t, decltype(&hb_raster_image_destroy)> painted(hb_raster_paint_render(paint.get()), &hb_raster_image_destroy);
    if (!painted) {
        throw std::bad_alloc();
    }

    // HarfBuzz paints premultiplied BGRA, which turns into straight RGBA with the top row first.
    hb_raster_extents_t rendered{};
    hb_raster_image_get_extents(painted.get(), &rendered);
    const std::uint8_t* source = hb_raster_image_get_buffer(painted.get());
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(rowBytes * static_cast<std::size_t>(height));
    for (std::size_t row = 0; row < static_cast<std::size_t>(height); ++row) {
        const std::uint8_t* from = source + (static_cast<std::size_t>(height) - 1U - row) * rendered.stride;
        std::uint8_t* to = pixels.data() + row * rowBytes;
        for (std::size_t pixel = 0; pixel < rowBytes; pixel += 4) {
            const unsigned int alpha = from[pixel + 3];
            if (alpha == 0) {
                continue;
            }
            to[pixel] = unpremultiply(from[pixel + 2], alpha);
            to[pixel + 1] = unpremultiply(from[pixel + 1], alpha);
            to[pixel + 2] = unpremultiply(from[pixel], alpha);
            to[pixel + 3] = static_cast<std::uint8_t>(alpha);
        }
    }

    graphics::Image image(width, height, std::move(pixels));
    image.bleed();
    return image;
}

} // namespace haylen::text
