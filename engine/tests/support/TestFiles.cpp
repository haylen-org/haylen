#include "support/TestFiles.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace haylen::test {

std::vector<std::uint8_t> TestFiles::bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> TestFiles::pngImage(int width, int height, std::uint32_t rgba) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = static_cast<std::uint8_t>((rgba >> 24U) & 0xFFU);
        pixels[offset + 1] = static_cast<std::uint8_t>((rgba >> 16U) & 0xFFU);
        pixels[offset + 2] = static_cast<std::uint8_t>((rgba >> 8U) & 0xFFU);
        pixels[offset + 3] = static_cast<std::uint8_t>(rgba & 0xFFU);
    }

    std::vector<std::uint8_t> encoded;
    // clang-format off
    stbi_write_png_to_func([](void* context, void* data, int size) {
        auto* output = static_cast<std::vector<std::uint8_t>*>(context);
        const auto* begin = static_cast<const std::uint8_t*>(data);
        output->insert(output->end(), begin, begin + size);
    }, &encoded, width, height, 4, pixels.data(), width * 4);
    // clang-format on
    return encoded;
}

} // namespace haylen::test
