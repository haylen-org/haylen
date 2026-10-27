#pragma once

#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// Checks at a frame that the view the app draws in covers its whole window and that the drawable has the pixels of that view, its bounds times its scale. Debug builds of UIKit run it every frame and log an error once each time the sizes part, so a layout that leaves the view or the drawable behind the window shows in the log.
class SurfaceCheck final {
  public:
    // The sizes of a frame: the window and the view in points, the scale of the view and the drawable in pixels.
    struct Sizes {
        math::Vec2 window;
        math::Vec2 view;
        float scale = 1.0F;
        math::Vec2 drawable;
    };

    // The pixels of an area in points at a scale, cut to whole pixels as UIKit and SDL size drawables.
    [[nodiscard]] static math::Vec2 toPixels(math::Vec2 points, float scale) noexcept;
    [[nodiscard]] static bool matches(const Sizes& sizes) noexcept;

    // Logs an error when the sizes part and the last frame matched, and returns whether it did.
    bool check(const Sizes& sizes);

  private:
    bool parted = false;
};

} // namespace haylen::platform
