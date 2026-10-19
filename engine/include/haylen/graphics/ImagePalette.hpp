#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics {

// The colors an image is made of: its palette, found by median cut over a sample of its visible pixels, its dominant color, and the swatches that an interface takes its colors from, such as the vivid and the muted colors of a cover.
class ImagePalette final {
  public:
    // A color of the palette with the share of the sampled pixels it stands for, from 0 to 1.
    struct Swatch {
        math::Color color;
        float population = 0.0F;
    };

    // The dominant color and the swatches that match six targets of lightness and saturation, each one empty when no color of the palette fits it. A palette that is empty has the transparent color as its dominant color.
    struct Swatches {
        math::Color dominant = math::Color::transparent();
        std::optional<math::Color> vibrant;
        std::optional<math::Color> darkVibrant;
        std::optional<math::Color> lightVibrant;
        std::optional<math::Color> muted;
        std::optional<math::Color> darkMuted;
        std::optional<math::Color> lightMuted;
    };

    static constexpr int kMaxColors = 64;

    // Returns up to `count` colors of the visible pixels of the image, the most common first, or none for an image without visible pixels. Throws `std::invalid_argument` for a count outside 1 to `kMaxColors`.
    [[nodiscard]] static std::vector<Swatch> extract(const Image& image, int count);

    [[nodiscard]] static Swatches choose(std::span<const Swatch> palette);

  private:
    using Sample = std::array<std::uint8_t, 3>;

    // A target of lightness and saturation, both in the HSL model, with the ranges a swatch must fall in.
    struct Target {
        float lightness = 0.5F;
        float minimumLightness = 0.0F;
        float maximumLightness = 1.0F;
        float saturation = 1.0F;
        float minimumSaturation = 0.0F;
        float maximumSaturation = 1.0F;
    };

    // The image is sampled on a grid of at most this many pixels per side, so the palette of a large image costs what a small one does.
    static constexpr int kSampleSide = 64;

    [[nodiscard]] static std::vector<Sample> sample(const Image& image);
    [[nodiscard]] static int widestChannel(std::span<const Sample> samples) noexcept;
    [[nodiscard]] static int channelRange(std::span<const Sample> samples, int channel) noexcept;
    [[nodiscard]] static math::Color average(std::span<const Sample> samples) noexcept;
    [[nodiscard]] static std::array<float, 2> lightnessAndSaturation(const math::Color& color) noexcept;
    [[nodiscard]] static std::optional<math::Color> find(std::span<const Swatch> palette, const Target& target, std::vector<bool>& used);
};

} // namespace haylen::graphics
