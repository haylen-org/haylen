#include "platform/linux/LinuxVisualColor.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace haylen::platform {

unsigned long LinuxVisualColor::toPixel(const math::Color& color, unsigned long redMask, unsigned long greenMask, unsigned long blueMask) noexcept {
    return toChannel(color.r, redMask) | toChannel(color.g, greenMask) | toChannel(color.b, blueMask);
}

// A channel spans the contiguous bits of its mask, so its largest value is the mask shifted down to its lowest bit.
unsigned long LinuxVisualColor::toChannel(float value, unsigned long mask) noexcept {
    if (mask == 0) {
        return 0;
    }
    const int shift = std::countr_zero(mask);
    const unsigned long largest = mask >> shift;
    const auto level = static_cast<unsigned long>(std::lround(std::clamp(value, 0.0F, 1.0F) * static_cast<float>(largest)));
    return level << shift;
}

} // namespace haylen::platform
