#pragma once

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
};

} // namespace haylen::test
