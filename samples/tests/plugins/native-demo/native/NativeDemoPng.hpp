#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Encodes RGBA pixels as a PNG file with stored deflate blocks, which needs no compressor.
class NativeDemoPng final {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(std::span<const std::uint8_t> pixels, int width, int height);

  private:
    // The largest stored block of deflate.
    static constexpr std::size_t kBlock = 65535;

    [[nodiscard]] static std::uint32_t crc(std::span<const std::uint8_t> bytes);
    static void put32(std::vector<std::uint8_t>& file, std::uint32_t value);

    // Appends a chunk of the type with its data and the checksum of both.
    static void appendChunk(std::vector<std::uint8_t>& file, std::string_view type, std::span<const std::uint8_t> data);
};
