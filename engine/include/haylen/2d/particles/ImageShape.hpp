#pragma once

#include <span>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
class Image;
}

namespace haylen::particles2d {

// The visible pixels of an image that emitters of the image shape spawn on, with their colors, so a sprite can burn, sparkle or break apart in its own shape. The points are pixel centers relative to the center of the source rectangle.
class ImageShape final {
  public:
    struct Options {
        // The part of the image to read, the whole image when it is empty.
        math::Rect source{};

        // Pixels whose alpha reaches the threshold count as visible.
        float alphaThreshold = 0.5F;
    };

    ImageShape(const graphics::Image& image, const Options& options);

    [[nodiscard]] std::span<const math::Vec2> getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] std::span<const math::Color> getColors() const noexcept {
        return colors;
    }

    // The size of the source rectangle in pixels.
    [[nodiscard]] math::Vec2 getSize() const noexcept {
        return size;
    }

  private:
    std::vector<math::Vec2> points;
    std::vector<math::Color> colors;
    math::Vec2 size{};
};

} // namespace haylen::particles2d
