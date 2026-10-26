#include "TestPortHost.hpp"

#include "TestPortRuntime.hpp"
#include "TestPortWindow.hpp"
#include "platform/Services.hpp"

namespace haylen::platform {

math::Vec2 TestPortHost::getFramebufferSize() const noexcept {
    return TestPortWindow::kSize;
}

float TestPortHost::getDpiScale() const noexcept {
    return 1.0F;
}

// The window of the test port always fills its screen.
bool TestPortHost::isFullscreen() const noexcept {
    return true;
}

void TestPortHost::setFullscreen(bool) {}

void TestPortHost::setTitle(std::string_view) {}

void TestPortHost::setCursor(Cursor) {}

void TestPortHost::setCursorVisible(bool) {}

void TestPortHost::setMouseLocked(bool) {}

void TestPortHost::setClipboard(std::string_view value) {
    clipboard = value;
}

std::string TestPortHost::getClipboard() const {
    return clipboard;
}

bool TestPortHost::canBeTransparent() const noexcept {
    return false;
}

std::unique_ptr<gpu::Device> TestPortHost::createGpuDevice() {
    return runtime.createDevice();
}

// The test port has no sound device, so its mixer plays into none.
audio::Mixer::Setup TestPortHost::getAudioSetup() const {
    return {.device = false};
}

TextInput& TestPortHost::getTextInput() noexcept {
    return Services::getTextInput();
}

void TestPortHost::closeWindow() {
    runtime.close();
}

void TestPortHost::pollGamepads(std::span<input::GamepadState>) {}

void TestPortHost::rumbleGamepad(std::size_t, float, float, float) {}

void TestPortHost::requestWindowClose() {
    runtime.requestClose();
}

void TestPortHost::setWindowFocusable(bool) {}

} // namespace haylen::platform
