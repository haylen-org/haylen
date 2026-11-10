#include "TestPortWindow.hpp"

#include "platform/PixelDensityCap.hpp"

namespace haylen::platform {

bool TestPortWindow::queryOpen() const noexcept {
    return open;
}

void* TestPortWindow::queryHandle() const noexcept {
    return nullptr;
}

void* TestPortWindow::queryDisplay() const noexcept {
    return nullptr;
}

void* TestPortWindow::queryActivity() const noexcept {
    return nullptr;
}

gpu::Surface::Size TestPortWindow::getSurfaceSize() const noexcept {
    return PixelDensityCap::apply(static_cast<int>(kSize.x), static_cast<int>(kSize.y), 1.0F, maxPixelDensity);
}

math::Vec2 TestPortWindow::queryFramebufferSize() const noexcept {
    const gpu::Surface::Size size = getSurfaceSize();
    return {static_cast<float>(size.width), static_cast<float>(size.height)};
}

float TestPortWindow::queryScale() const noexcept {
    return PixelDensityCap::getShare(getSurfaceSize());
}

} // namespace haylen::platform
