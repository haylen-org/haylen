#pragma once

#include <array>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::gpu {

// The quarter turns, clockwise, by which the images of a swapchain hold what the window shows, for a window system that turns the images back as it shows them, such as Android on a phone held across its panel. A swapchain pass into such images draws turned: the device turns the viewports and scissors that the renderer gives in the pixels of the window, and the renderer turns the positions in clip space that its vertex stages write.
class SurfaceTurn final {
  public:
    using Matrix = std::array<float, 16>;

    // Turns a rectangle of pixels of the window, whose size is `window`, into the pixels of the images.
    [[nodiscard]] static math::Rect turnPixels(const math::Rect& pixels, math::Vec2 window, int turns) noexcept;

    // Turns a matrix whose columns follow one another into clip space, so the positions it writes land turned in the images.
    [[nodiscard]] static Matrix turnClip(const Matrix& matrix, int turns) noexcept;
};

} // namespace haylen::gpu
