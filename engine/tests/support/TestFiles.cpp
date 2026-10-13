#include "support/TestFiles.hpp"

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::test {

std::vector<std::uint8_t> TestFiles::bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> TestFiles::pngImage(int width, int height, std::uint32_t rgba) {
    return graphics::Image(width, height, math::Color::fromHex(rgba)).encodePng();
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
