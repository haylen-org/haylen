#include "graphics/SvgDocument.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#include "haylen/graphics/VectorImage.hpp"

#include <nanosvg.h>
#include <nanosvgrast.h>

namespace haylen::graphics {

SvgDocument::SvgDocument(std::unique_ptr<NSVGimage, void (*)(NSVGimage*)> parsed, math::Vec2 documentSize) noexcept : VectorImageResource(documentSize), document(std::move(parsed)) {}

// The rasterizer of nanosvg spreads the colors of the edges into the transparent pixels around them as it turns its premultiplied pixels into straight ones.
Image SvgDocument::rasterize(float scale) const {
    const auto [width, height] = VectorImage::getRasterSize(getSize(), scale);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    const std::unique_ptr<NSVGrasterizer, decltype(&nsvgDeleteRasterizer)> rasterizer(nsvgCreateRasterizer(), &nsvgDeleteRasterizer);
    nsvgRasterize(rasterizer.get(), document.get(), 0.0F, 0.0F, scale, pixels.data(), width, height, width * 4);
    return {width, height, std::move(pixels)};
}

} // namespace haylen::graphics
