#include "haylen/graphics/VectorImage.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "graphics/SvgDocument.hpp"

#include <nanosvg.h>

namespace haylen::graphics {

// The reader changes the text it reads, which therefore is a copy, and `currentColor` becomes white, which the color of a draw then tints.
VectorImage VectorImage::parse(std::span<const std::uint8_t> bytes) {
    std::string text(bytes.begin(), bytes.end());
    constexpr std::string_view kCurrent = "currentColor";
    constexpr std::string_view kWhite = "#FFFFFF";
    for (std::size_t found = text.find(kCurrent); found != std::string::npos; found = text.find(kCurrent, found + kWhite.size())) {
        text.replace(found, kCurrent.size(), kWhite);
    }

    std::unique_ptr<NSVGimage, void (*)(NSVGimage*)> document{nsvgParse(text.data(), "px", 96.0F), &nsvgDelete};
    if (!document || !(document->width > 0.0F && document->height > 0.0F)) {
        throw std::invalid_argument("The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.");
    }
    const math::Vec2 size{document->width, document->height};
    return VectorImage(std::make_shared<SvgDocument>(std::move(document), size));
}

math::Vec2 VectorImage::getSize() const {
    return resource->getSize();
}

std::uint32_t VectorImage::getId() const {
    return resource->getId();
}

std::pair<int, int> VectorImage::getRasterSize(math::Vec2 size, float scale) noexcept {
    return {std::max(1, static_cast<int>(std::ceil(size.x * scale))), std::max(1, static_cast<int>(std::ceil(size.y * scale)))};
}

// The size is checked in floating point before it becomes whole pixels, so an image of any size fails with the reason instead of overflowing.
Image VectorImage::rasterize(float scale) const {
    const math::Vec2 extent = resource->getSize() * scale;
    if (!(std::ceil(extent.x) <= static_cast<float>(Image::kMaxSide) && std::ceil(extent.y) <= static_cast<float>(Image::kMaxSide))) {
        throw std::invalid_argument("The raster of the vector image at this scale would have more than " + std::to_string(Image::kMaxSide) + " pixels on a side. Rasterize it at a smaller scale.");
    }

    return resource->rasterize(scale);
}

} // namespace haylen::graphics
