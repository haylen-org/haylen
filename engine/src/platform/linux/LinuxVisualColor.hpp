#pragma once

#include "haylen/math/Color.hpp"

namespace haylen::platform {

// A color as the pixel of a true color visual of X11, whose masks tell the bits of each channel.
class LinuxVisualColor final {
  public:
    [[nodiscard]] static unsigned long toPixel(const math::Color& color, unsigned long redMask, unsigned long greenMask, unsigned long blueMask) noexcept;

  private:
    [[nodiscard]] static unsigned long toChannel(float value, unsigned long mask) noexcept;
};

} // namespace haylen::platform
