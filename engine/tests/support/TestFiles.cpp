#include "support/TestFiles.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::test {

std::vector<std::uint8_t> TestFiles::bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> TestFiles::pngImage(int width, int height, std::uint32_t rgba) {
    return graphics::Image(width, height, math::Color::fromHex(rgba)).encodePng();
}

std::vector<std::uint8_t> TestFiles::wav(std::uint32_t sampleRate, std::uint16_t channels, const std::vector<float>& samples) {
    std::vector<std::uint8_t> file;
    // clang-format off
    const auto append = [&file](std::uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index) {
            file.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFFU));
        }
    };
    // clang-format on
    const auto appendText = [&file](const char* text) { file.insert(file.end(), text, text + 4); };
    const auto dataSize = static_cast<std::uint32_t>(samples.size() * 2);

    appendText("RIFF");
    append(36 + dataSize, 4);
    appendText("WAVE");
    appendText("fmt ");
    append(16, 4);
    append(1, 2);
    append(channels, 2);
    append(sampleRate, 4);
    append(sampleRate * channels * 2, 4);
    append(channels * 2U, 2);
    append(16, 2);
    appendText("data");
    append(dataSize, 4);
    for (const float sample : samples) {
        const auto value = static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767.0F));
        append(static_cast<std::uint16_t>(value), 2);
    }
    return file;
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
