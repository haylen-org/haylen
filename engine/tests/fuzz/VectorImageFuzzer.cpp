#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

#include "haylen/graphics/VectorImage.hpp"

// The entry point libFuzzer calls with each input it generates, which must either read into a vector image that rasterizes within 64 pixels on its longest side or fail with the error of text that is not an SVG document with a size.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        const haylen::graphics::VectorImage image = haylen::graphics::VectorImage::parse(std::span(data, size));
        const haylen::math::Vec2 extent = image.getSize();
        (void)image.rasterize(64.0F / std::max(extent.x, extent.y));
    } catch (const std::invalid_argument&) {}
    return 0;
}
