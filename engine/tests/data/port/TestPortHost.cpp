#include "TestPortHost.hpp"

#include "TestPortRuntime.hpp"
#include "TestPortWindow.hpp"
#include "haylen/platform/PixelDensityCap.hpp"

namespace haylen::platform {

math::Vec2 TestPortHost::getFramebufferSize() const noexcept {
    const gpu::Surface::Size size = runtime.getWindow().getSurfaceSize();
    return {static_cast<float>(size.width), static_cast<float>(size.height)};
}

float TestPortHost::getDpiScale() const noexcept {
    return PixelDensityCap::getShare(runtime.getWindow().getSurfaceSize());
}

// The window of the test port always fills its screen.
bool TestPortHost::isFullscreen() const noexcept {
    return true;
}

void TestPortHost::setFullscreen(bool value, std::function<void(SystemAnswer answer)> callback) {
    if (!value) {
        callback({.state = SystemAnswer::State::Unsupported, .message = "The window of the test port always fills its screen."});
        return;
    }

    callback({.state = SystemAnswer::State::Granted});
}

// The screen of the test port shows no title and no cursor.
void TestPortHost::setTitle(std::string_view) {}

void TestPortHost::setCursor(Window::Cursor) {}

void TestPortHost::setMouseMode(Window::MouseMode value, std::function<void(SystemAnswer answer)> callback) {
    if (value != Window::MouseMode::Normal) {
        callback({.state = SystemAnswer::State::Unsupported, .message = "The test port has no pointer to hide, lock or confine."});
        return;
    }

    callback({.state = SystemAnswer::State::Granted});
}

// The test port has no clipboard of a system, so the window keeps its own.
void TestPortHost::setClipboard(std::string_view value, std::function<void(SystemAnswer answer)> callback) {
    clipboard = value;
    callback({.state = SystemAnswer::State::Granted});
}

std::string TestPortHost::getClipboard() const {
    return clipboard;
}

void TestPortHost::readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback) {
    callback({.state = SystemAnswer::State::Granted}, clipboard);
}

bool TestPortHost::canBeTransparent() const noexcept {
    return false;
}

TextInput& TestPortHost::getTextInput() noexcept {
    return textInput;
}

bool TestPortHost::prepareGpuDevice() {
    return true;
}

std::unique_ptr<gpu::Device> TestPortHost::createGpuDevice() {
    return runtime.createDevice();
}

void TestPortHost::requestClose() {
    runtime.requestClose();
}

void TestPortHost::close() {
    runtime.close();
}

// The test port runs on a desktop, whose processes end when their app quits.
bool TestPortHost::canQuit() const noexcept {
    return true;
}

// The test port shows no splash of its own.
void TestPortHost::endSplashScreen(float) {}

// The window of the test port has no events of its own, so a halt keeps nothing of the platform waiting.
bool TestPortHost::pumpHaltedEvents() {
    return false;
}

// The test port has no controllers.
void TestPortHost::pollGamepads(std::span<input::GamepadState>) {}

void TestPortHost::rumbleGamepad(std::size_t, float, float, float) {}

void TestPortHost::setGamepadMotion(input::GamepadMotion::Sensors) {}

void TestPortHost::setFocusable(bool) {}

// The test port has no screen saver, so its screen stays on.
void TestPortHost::setScreenSaverSuspended(bool, std::function<void(SystemAnswer answer)> callback) {
    callback({.state = SystemAnswer::State::Granted});
}

// The runtime of the test port plays frames of a fixed length that no display paces.
void TestPortHost::applyFrameRate(int) {}

void TestPortHost::applyMaxPixelDensity(float cap) {
    runtime.setMaxPixelDensity(cap);
}

} // namespace haylen::platform
