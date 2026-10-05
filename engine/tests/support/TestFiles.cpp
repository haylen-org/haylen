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

std::vector<std::uint8_t> TestFiles::randomBytes(std::size_t size, std::uint64_t seed) {
    std::vector<std::uint8_t> bytes(size);
    std::uint64_t state = seed;
    for (std::size_t offset = 0; offset < size; offset += sizeof(std::uint64_t)) {
        state += 0x9E3779B97F4A7C15;
        std::uint64_t value = state;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EB;
        value ^= value >> 31;
        for (std::size_t index = 0; index < sizeof(value) && offset + index < size; ++index) {
            bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }
    return bytes;
}

} // namespace haylen::test
