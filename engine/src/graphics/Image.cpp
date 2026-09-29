#include "haylen/graphics/Image.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace haylen::graphics {

std::size_t Image::byteCount(int columns, int rows) {
    if (columns < 0 || rows < 0) {
        throw std::invalid_argument("Image dimensions cannot be negative.");
    }

    // The byte count must fit in a pointer difference, which also keeps it from overflowing on 32-bit platforms.
    const auto columnCount = static_cast<std::size_t>(columns);
    const auto rowCount = static_cast<std::size_t>(rows);
    if (rowCount != 0 && columnCount > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / 4U / rowCount) {
        throw std::invalid_argument("Image dimensions are too large to hold in memory.");
    }
    return columnCount * rowCount * 4U;
}

Image::Image(int imageWidth, int imageHeight, math::Color fill) : width(imageWidth), height(imageHeight), pixels(byteCount(imageWidth, imageHeight)) {
    const std::uint32_t packed = fill.toRgba8();
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = static_cast<std::uint8_t>(packed & 0xFFU);
        pixels[offset + 1] = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
        pixels[offset + 2] = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
        pixels[offset + 3] = static_cast<std::uint8_t>((packed >> 24U) & 0xFFU);
    }
}

Image::Image(int imageWidth, int imageHeight, std::vector<std::uint8_t> data) : width(imageWidth), height(imageHeight), pixels(std::move(data)) {
    if (pixels.size() != byteCount(imageWidth, imageHeight)) {
        throw std::invalid_argument("Image pixel data does not match its dimensions.");
    }
}

Image Image::decode(std::span<const std::uint8_t> encoded) {
    int decodedWidth = 0;
    int decodedHeight = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &decodedWidth, &decodedHeight, &channels, STBI_rgb_alpha);
    if (decoded == nullptr) {
        throw std::runtime_error(std::string("Image could not be decoded: ") + stbi_failure_reason());
    }

    std::vector<std::uint8_t> data(decoded, decoded + byteCount(decodedWidth, decodedHeight));
    stbi_image_free(decoded);
    return Image(decodedWidth, decodedHeight, std::move(data));
}

math::Color Image::getPixel(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return math::Color::transparent();
    }
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return math::Color::fromRgba8(pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]);
}

void Image::setPixel(int x, int y, math::Color color) noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    const std::uint32_t packed = color.toRgba8();
    pixels[offset + 0] = static_cast<std::uint8_t>(packed & 0xFFU);
    pixels[offset + 1] = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
    pixels[offset + 2] = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
    pixels[offset + 3] = static_cast<std::uint8_t>((packed >> 24U) & 0xFFU);
}

} // namespace haylen::graphics
