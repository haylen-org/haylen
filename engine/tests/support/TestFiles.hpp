#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace haylen::test {

// Builds the contents of files that tests put in packages.
class TestFiles final {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> bytes(const std::string& text);

    // Encodes a PNG image of one color, given as `0xRRGGBBAA`.
    [[nodiscard]] static std::vector<std::uint8_t> pngImage(int width, int height, std::uint32_t rgba);

    // Encodes interleaved samples in the range -1 to 1 as a 16-bit PCM WAV file.
    [[nodiscard]] static std::vector<std::uint8_t> wav(std::uint32_t sampleRate, std::uint16_t channels, const std::vector<float>& samples);

    // Returns bytes that look random but are the same for the same seed, from SplitMix64.
    [[nodiscard]] static std::vector<std::uint8_t> randomBytes(std::size_t size, std::uint64_t seed);
};

} // namespace haylen::test
