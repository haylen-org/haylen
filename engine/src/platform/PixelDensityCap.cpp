#include "platform/PixelDensityCap.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::platform {

gpu::Surface::Size PixelDensityCap::apply(int windowWidth, int windowHeight, float density, float cap) noexcept {
    gpu::Surface::Size size{.width = windowWidth, .height = windowHeight, .windowWidth = windowWidth, .windowHeight = windowHeight};
    if (cap <= 0.0F || density <= cap || windowWidth <= 0 || windowHeight <= 0) {
        return size;
    }

    const float share = cap / density;
    size.width = std::max(1, static_cast<int>(std::lround(static_cast<float>(windowWidth) * share)));
    size.height = std::max(1, static_cast<int>(std::lround(static_cast<float>(windowHeight) * share)));
    return size;
}

float PixelDensityCap::getShare(const gpu::Surface::Size& size) noexcept {
    return size.isScaled() ? static_cast<float>(size.width) / static_cast<float>(size.windowWidth) : 1.0F;
}

} // namespace haylen::platform
