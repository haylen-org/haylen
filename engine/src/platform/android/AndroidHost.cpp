#include "platform/android/AndroidHost.hpp"

#include "platform/NativeWindow.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidRuntime.hpp"
#include "platform/android/JavaBridge.hpp"

namespace haylen::platform {

math::Vec2 AndroidHost::getFramebufferSize() const noexcept {
    return NativeWindow::getFramebufferSize();
}

// The activity reports the window pixels per point, the density of the display, and the window the framebuffer pixels per window pixel.
float AndroidHost::getDpiScale() const noexcept {
    return AndroidActivity::getDensity() * NativeWindow::getScale();
}

void AndroidHost::setClipboard(std::string_view value) {
    JavaBridge::setClipboard(value);
}

std::string AndroidHost::getClipboard() const {
    return JavaBridge::getClipboard();
}

std::unique_ptr<gpu::Device> AndroidHost::createGpuDevice() {
    return runtime.getGraphics().createDevice();
}

void AndroidHost::closeWindow() {
    runtime.quit();
}

void AndroidHost::pollGamepads(std::span<input::GamepadState> gamepads) {
    AndroidGamepads::poll(gamepads);
}

void AndroidHost::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    AndroidGamepads::rumble(index, low, high, seconds);
}

void AndroidHost::requestWindowClose() {
    runtime.requestQuit();
}

} // namespace haylen::platform
