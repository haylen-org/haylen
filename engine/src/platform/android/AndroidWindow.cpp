#include "platform/android/AndroidWindow.hpp"

#include "platform/PixelDensityCap.hpp"
#include "platform/android/AndroidActivity.hpp"

namespace haylen::platform {

// A buffer geometry of the very size of the window shows artifacts on some devices, so only a framebuffer smaller than the window sets one, and a window that gets every pixel back resets it.
void AndroidWindow::open(ANativeWindow& window, int width, int height) noexcept {
    windowWidth = width;
    windowHeight = height;
    const gpu::Surface::Size size = PixelDensityCap::apply(width, height, AndroidActivity::getDensity(), maxPixelDensity);
    if (size.isScaled()) {
        ANativeWindow_setBuffersGeometry(&window, size.width, size.height, 0);
        geometrySet = true;
    } else if (geometrySet) {
        ANativeWindow_setBuffersGeometry(&window, 0, 0, 0);
        geometrySet = false;
    }

    framebufferWidth.store(size.width);
    framebufferHeight.store(size.height);
    scale.store(PixelDensityCap::getShare(size));
    handle.store(&window);
}

void AndroidWindow::refresh() noexcept {
    if (ANativeWindow* window = handle.load()) {
        open(*window, windowWidth, windowHeight);
    }
}

math::Vec2 AndroidWindow::queryFramebufferSize() const noexcept {
    return {static_cast<float>(framebufferWidth.load()), static_cast<float>(framebufferHeight.load())};
}

} // namespace haylen::platform
