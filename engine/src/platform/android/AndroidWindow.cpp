#include "platform/android/AndroidWindow.hpp"

namespace haylen::platform {

// A buffer geometry of the very size of the window shows artifacts on some devices, so only a framebuffer smaller than the window sets one.
void AndroidWindow::open(ANativeWindow& window, int width, int height) noexcept {
    const int bufferWidth = fullDensity ? width : width / 2;
    const int bufferHeight = fullDensity ? height : height / 2;
    if (!fullDensity) {
        ANativeWindow_setBuffersGeometry(&window, bufferWidth, bufferHeight, 0);
    }
    framebufferWidth.store(bufferWidth);
    framebufferHeight.store(bufferHeight);
    scale.store(width > 0 ? static_cast<float>(bufferWidth) / static_cast<float>(width) : 1.0F);
    handle.store(&window);
}

math::Vec2 AndroidWindow::queryFramebufferSize() const noexcept {
    return {static_cast<float>(framebufferWidth.load()), static_cast<float>(framebufferHeight.load())};
}

} // namespace haylen::platform
