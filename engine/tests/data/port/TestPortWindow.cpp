#include "TestPortWindow.hpp"

#include "haylen/platform/PixelDensityCap.hpp"

namespace haylen::platform {

gpu::Surface::Size TestPortWindow::getSurfaceSize() const noexcept {
    return PixelDensityCap::apply(static_cast<int>(kSize.x), static_cast<int>(kSize.y), 1.0F, maxPixelDensity);
}

} // namespace haylen::platform
