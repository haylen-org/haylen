#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/graphics/Image.hpp"

namespace haylen::graphics {

// A short text that stands for an image while the image itself loads, such as in a list of products that a server sends: the low frequencies of the colors of the image, a few cosines on each axis, which `decode` turns into a blurred image of any size. The text uses only letters, digits, `-` and `_`, so it fits in URLs and JSON as it is.
class ImagePlaceholder final {
  public:
    // How many cosines the text keeps on each axis, from 1 to 9. More of them keep more of the shapes of the image in a longer text.
    struct Options {
        int columns = 4;
        int rows = 3;
    };

    static constexpr int kMaxComponents = 9;
    static constexpr int kMaxSize = 512;

    // Throws `std::invalid_argument` for an image without pixels or options out of range.
    [[nodiscard]] static std::string encode(const Image& image, Options options);

    // Throws `std::invalid_argument` for a text that is not a placeholder, or for a size out of 1 to `kMaxSize`.
    [[nodiscard]] static Image decode(std::string_view code, int width, int height);

  private:
    using Linear = std::array<float, 3>;

    // The image is averaged into a grid of at most this many cells per side before its cosines add up, so a large image costs what a small one does.
    static constexpr int kGridSide = 32;
    static constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    // Each of the three channels of a cosine is one of this many levels, three of which make two bytes.
    static constexpr int kLevels = 19;

    [[nodiscard]] static float toLinear(std::uint8_t channel) noexcept;
    [[nodiscard]] static std::uint8_t toByte(float linear) noexcept;
    [[nodiscard]] static std::vector<Linear> average(const Image& image, int columns, int rows);
    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] static std::vector<std::uint8_t> fromText(std::string_view text);
};

} // namespace haylen::graphics
