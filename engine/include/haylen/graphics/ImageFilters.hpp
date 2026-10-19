#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics {

// Effects that change the pixels of an image on the CPU, for art that code prepares before it becomes a texture, such as a blurred backdrop behind a dialog, the gray card of a locked level or the outline of a sprite. Every effect works in place on the whole image and keeps the alpha of the pixels, except the outline, which draws around the pixels that show. The effects that mix pixels, the blur, the sharpening and the pixelation, mix their colors premultiplied by their alpha, so transparent pixels never darken the edges.
class ImageFilters final {
  public:
    // A change of the colors, applied in this order: the hue turns by its turns, the saturation scales the distance from gray, the contrast scales the distance from the middle gray, and the brightness adds to every channel.
    struct Adjustment {
        float brightness = 0.0F;
        float contrast = 1.0F;
        float saturation = 1.0F;
        float hue = 0.0F;
    };

    static constexpr float kMaxRadius = 64.0F;
    static constexpr float kMaxThickness = 32.0F;

    // A blur as soft as a Gaussian of the radius in pixels, made of three box blurs. A radius of 0 changes nothing, and one out of 0 to `kMaxRadius` throws `std::invalid_argument`.
    static void blur(Image& image, float radius);

    // Raises the contrast of the details by the amount, 1 for a clear sharpening, by subtracting a blur of one pixel. A negative amount throws `std::invalid_argument`.
    static void sharpen(Image& image, float amount);

    static void adjust(Image& image, const Adjustment& adjustment) noexcept;

    // Moves the colors toward their gray by the amount, from 0 to 1.
    static void grayscale(Image& image, float amount) noexcept;
    static void invert(Image& image) noexcept;

    // Moves the colors toward the color by the amount, from 0 to 1, weighed by the alpha of the color.
    static void tint(Image& image, math::Color color, float amount) noexcept;

    // Fills every square of the size in pixels with the average of its colors. A size below 1 throws `std::invalid_argument`.
    static void pixelate(Image& image, int size);

    // Draws an outline of the color and the thickness in pixels around the pixels that show, under them and inside the image, solid within the thickness and fading over the pixel past it. A thickness out of 0 to `kMaxThickness` throws `std::invalid_argument`.
    static void outline(Image& image, math::Color color, float thickness);

  private:
    using Pixels = std::vector<std::array<float, 4>>;

    [[nodiscard]] static std::uint8_t toByte(float value) noexcept;
    [[nodiscard]] static Pixels premultiplied(const Image& image);
    static void store(Image& image, const Pixels& pixels) noexcept;
    static void boxBlur(Pixels& pixels, Pixels& scratch, int width, int height, int radius);
    [[nodiscard]] static std::array<int, 3> boxRadii(float sigma) noexcept;
};

} // namespace haylen::graphics
