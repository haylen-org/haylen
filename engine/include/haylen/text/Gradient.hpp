#pragma once

#include <array>
#include <cstddef>
#include <numbers>
#include <span>

#include "haylen/graphics/Gradient.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::text {

// The colors that fill the glyphs of a text from stop to stop along a direction across each of its lines, which runs from the top of the line down unless it names another angle. The colors blend premultiplied by their alpha and multiply the color of the text, so the alpha of the text still fades them. Only glyphs of fonts with a distance field fill with a gradient, while bitmap and color glyphs keep their color, and outlines, shadows and glows keep theirs.
class Gradient final {
  public:
    using Stop = graphics::Gradient::Stop;

    // The text shader reads the stops of every glyph from one record, which holds up to four.
    static constexpr std::size_t kMaxStops = 4;
    static constexpr float kDown = std::numbers::pi_v<float> * 0.5F;

    // Throws `std::invalid_argument` without stops, with more than four, for offsets out of 0 to 1 or out of order, or for an angle that is not finite.
    explicit Gradient(std::span<const Stop> gradientStops, float gradientAngle = kDown);

    // Spreads the colors evenly from 0 to 1.
    [[nodiscard]] static Gradient fromColors(std::span<const math::Color> colors, float gradientAngle = kDown);

    [[nodiscard]] std::span<const Stop> getStops() const noexcept {
        return {stops.data(), count};
    }

    // The direction of the gradient in radians clockwise from the x axis.
    [[nodiscard]] float getAngle() const noexcept {
        return angle;
    }
    void setAngle(float value);

  private:
    std::array<Stop, kMaxStops> stops{};
    std::size_t count = 0;
    float angle = kDown;
};

} // namespace haylen::text
