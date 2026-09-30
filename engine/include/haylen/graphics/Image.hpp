#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Color.hpp"

namespace haylen::graphics {

// CPU-side RGBA8 pixels, usually decoded on a worker thread before becoming a texture.
class Image final {
  public:
    Image() = default;
    Image(int imageWidth, int imageHeight, math::Color fill = math::Color::transparent());
    Image(int imageWidth, int imageHeight, std::vector<std::uint8_t> data);

    // Decodes PNG, JPG, TGA, BMP and GIF data. Throws `std::runtime_error` when the data is not a supported image.
    [[nodiscard]] static Image decode(std::span<const std::uint8_t> encoded);

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }
    [[nodiscard]] bool isEmpty() const noexcept {
        return pixels.empty();
    }
    [[nodiscard]] std::span<const std::uint8_t> getPixels() const noexcept {
        return pixels;
    }
    [[nodiscard]] std::span<std::uint8_t> getPixels() noexcept {
        return pixels;
    }

    [[nodiscard]] math::Color getPixel(int x, int y) const noexcept;
    void setPixel(int x, int y, math::Color color) noexcept;

  private:
    [[nodiscard]] static std::size_t byteCount(int columns, int rows);

    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

} // namespace haylen::graphics
