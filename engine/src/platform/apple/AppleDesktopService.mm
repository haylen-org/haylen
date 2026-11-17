#import "platform/apple/AppleDesktopService.hpp"

#import "platform/NativeWindow.hpp"
#import "platform/apple/AppleDesktop.hpp"

namespace haylen::platform {

void AppleDesktopService::setWindowStyle(const WindowStyle& value) {
    AppleDesktop::setStyle(value);
}

// AppKit shows the frames that the app drew, so the window needs no color of its own.
void AppleDesktopService::setWindowBackground(const math::Color&) {}

void AppleDesktopService::setWindowFrame(const math::Rect& value) {
    AppleDesktop::setFrame(value);
}

void AppleDesktopService::setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) {
    AppleDesktop::setPassthrough(mode, regions);
}

void AppleDesktopService::startWindowDrag() {
    AppleDesktop::startDrag();
}

void AppleDesktopService::watchWindow() {
    AppleDesktop::watch();
}

void AppleDesktopService::updateWindow() {
    AppleDesktop::update();
}

HaylenNativeWindow AppleDesktopService::getNativeWindow() {
    return {.handle = NativeWindow::getHandle(), .display = nullptr};
}

} // namespace haylen::platform
