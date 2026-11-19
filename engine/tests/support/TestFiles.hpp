#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::test {

// Builds the contents of files that tests put in packages, and reads, writes and damages the files of a test on disk. A file that the system refuses throws, so a test never goes on with bytes it did not get.
class TestFiles final {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> read(const std::filesystem::path& file);
    [[nodiscard]] static std::string readText(const std::filesystem::path& file);
    static void write(const std::filesystem::path& file, std::span<const std::uint8_t> bytes);
    static void write(const std::filesystem::path& file, std::string_view text);
    static void append(const std::filesystem::path& file, std::string_view text);

    // Flips the lowest bit of the byte at an offset, which damages a file without changing its size.
    static void damage(const std::filesystem::path& file, std::uintmax_t offset);

    [[nodiscard]] static std::vector<std::uint8_t> bytes(const std::string& text);

    // Returns the bytes of a file as the text that a package of files in memory holds.
    [[nodiscard]] static std::string toText(std::span<const std::uint8_t> data);

    // Encodes a PNG image of one color, given as `0xRRGGBBAA`.
    [[nodiscard]] static std::vector<std::uint8_t> pngImage(int width, int height, std::uint32_t rgba);

    // Encodes interleaved samples in the range -1 to 1 as a 16-bit PCM WAV file.
    [[nodiscard]] static std::vector<std::uint8_t> wav(std::uint32_t sampleRate, std::uint16_t channels, const std::vector<float>& samples);

    // Returns bytes that look random but are the same for the same seed, from SplitMix64.
    [[nodiscard]] static std::vector<std::uint8_t> randomBytes(std::size_t size, std::uint64_t seed);
};

} // namespace haylen::test
