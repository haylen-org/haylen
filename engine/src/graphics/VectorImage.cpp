#include "haylen/graphics/VectorImage.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/VectorImageResource.hpp"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

namespace haylen::graphics {

// The reader changes the text it reads, which therefore is a copy, and `currentColor` becomes white, which the color of a draw then tints.
VectorImage VectorImage::parse(std::span<const std::uint8_t> bytes) {
    std::string text(bytes.begin(), bytes.end());
    constexpr std::string_view kCurrent = "currentColor";
    constexpr std::string_view kWhite = "#FFFFFF";
    for (std::size_t found = text.find(kCurrent); found != std::string::npos; found = text.find(kCurrent, found + kWhite.size())) {
        text.replace(found, kCurrent.size(), kWhite);
    }

    auto resource = std::make_shared<VectorImageResource>();
    resource->document = {nsvgParse(text.data(), "px", 96.0F), &nsvgDelete};
    if (!resource->document || !(resource->document->width > 0.0F && resource->document->height > 0.0F)) {
        throw std::invalid_argument("The bytes are not an SVG document with a size. Give its root element a view box, or a width and a height.");
    }
    resource->size = {resource->document->width, resource->document->height};
    return VectorImage(std::move(resource));
}

math::Vec2 VectorImage::getSize() const {
    return resource->size;
}

std::uint32_t VectorImage::getId() const {
    return resource->id;
}

std::pair<int, int> VectorImage::getRasterSize(math::Vec2 size, float scale) noexcept {
    return {std::max(1, static_cast<int>(std::ceil(size.x * scale))), std::max(1, static_cast<int>(std::ceil(size.y * scale)))};
}

Image VectorImage::rasterize(float scale) const {
    const auto [width, height] = getRasterSize(resource->size, scale);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    const std::unique_ptr<NSVGrasterizer, decltype(&nsvgDeleteRasterizer)> rasterizer(nsvgCreateRasterizer(), &nsvgDeleteRasterizer);
    nsvgRasterize(rasterizer.get(), resource->document.get(), 0.0F, 0.0F, scale, pixels.data(), width, height, width * 4);
    return {width, height, std::move(pixels)};
}

} // namespace haylen::graphics
