#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {

// Colors that blend from stop to stop across an area, along a direction, out from a center or around it, painted into images on the CPU for backgrounds, buttons, skies and health bars that a texture then shows, such as through a shape. The colors blend premultiplied by their alpha, so a stop that fades out never darkens its neighbours.
class Gradient final {
  public:
    // A linear gradient runs along its angle across the whole area, a radial one out from its center to its radius, and a conic one around its center clockwise from its angle.
    enum class Kind : std::uint8_t {
        Linear,
        Radial,
        Conic,
    };

    struct Stop {
        float offset = 0.0F;
        math::Color color;
    };

    // Throws `std::invalid_argument` without stops or for offsets out of 0 to 1 or out of order.
    explicit Gradient(std::vector<Stop> gradientStops, Kind gradientKind = Kind::Linear);

    // Spreads the colors evenly from 0 to 1. Throws `std::invalid_argument` without colors.
    [[nodiscard]] static Gradient fromColors(std::span<const math::Color> colors, Kind gradientKind = Kind::Linear);

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }
    [[nodiscard]] const std::vector<Stop>& getStops() const noexcept {
        return stops;
    }

    // The direction of a linear gradient and the start of a conic one, in radians clockwise from the x axis.
    [[nodiscard]] float getAngle() const noexcept {
        return angle;
    }
    void setAngle(float value) noexcept {
        angle = value;
    }

    // The center of a radial or conic gradient as a share of the area on each axis.
    [[nodiscard]] math::Vec2 getCenter() const noexcept {
        return center;
    }
    void setCenter(math::Vec2 value) noexcept {
        center = value;
    }

    // The radius of a radial gradient as a share of the width and of the height of the area, so the default reaches the middle of every side. Throws `std::invalid_argument` for a radius that is not above 0.
    [[nodiscard]] math::Vec2 getRadius() const noexcept {
        return radius;
    }
    void setRadius(math::Vec2 value);

    // Spreads the rounding of every pixel to 8 bits with an ordered pattern, so wide gradients show no bands.
    [[nodiscard]] bool isDithered() const noexcept {
        return dithered;
    }
    void setDithered(bool value) noexcept {
        dithered = value;
    }

    // The straight color at a position from 0 to 1 along the gradient, the first or last color past its stops.
    [[nodiscard]] math::Color getColor(float at) const noexcept;

    // Replaces the pixels of the area inside the image with the gradient laid over the whole area.
    void paint(Image& image, const Image::Area& area) const;

    static constexpr std::array<std::pair<std::string_view, Kind>, 3> kKindNames{{{"linear", Kind::Linear}, {"radial", Kind::Radial}, {"conic", Kind::Conic}}};

  private:
    static constexpr std::array<float, 16> kDither{0.0F, 8.0F, 2.0F, 10.0F, 12.0F, 4.0F, 14.0F, 6.0F, 3.0F, 11.0F, 1.0F, 9.0F, 15.0F, 7.0F, 13.0F, 5.0F};

    [[nodiscard]] float positionOf(math::Vec2 point, math::Vec2 size) const noexcept;

    std::vector<Stop> stops;
    Kind kind = Kind::Linear;
    float angle = 0.0F;
    math::Vec2 center{0.5F, 0.5F};
    math::Vec2 radius{0.5F, 0.5F};
    bool dithered = true;
};

} // namespace haylen::graphics
