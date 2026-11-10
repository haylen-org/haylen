#include "TestPortHost.hpp"

#include "TestPortRuntime.hpp"
#include "TestPortWindow.hpp"
#include "platform/PixelDensityCap.hpp"
#include "platform/Services.hpp"

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

void TestPortHost::setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (!value) {
        callback({.state = Permission::State::Unsupported, .message = "The window of the test port always fills its screen."});
        return;
    }
    callback({.state = Permission::State::Granted});
}

void TestPortHost::setTitle(std::string_view) {}

void TestPortHost::setCursor(Cursor) {}

void TestPortHost::setCursorVisible(bool) {}

void TestPortHost::setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (value) {
        callback({.state = Permission::State::Unsupported, .message = "The test port has no pointer to lock."});
        return;
    }
    callback({.state = Permission::State::Granted});
}

void TestPortHost::setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) {
    clipboard = value;
    callback({.state = Permission::State::Granted});
}

std::string TestPortHost::getClipboard() const {
    return clipboard;
}

void TestPortHost::readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) {
    callback({.state = Permission::State::Granted}, clipboard);
}

bool TestPortHost::canBeTransparent() const noexcept {
    return false;
}

bool TestPortHost::prepareGpuDevice() {
    return true;
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

void TestPortHost::endSplashScreen(float fadeOutSeconds) {
    Services::endSplashScreen(fadeOutSeconds);
}

// The window of the test port has no events of its own, so a halt keeps nothing of the platform waiting.
bool TestPortHost::pumpHaltedEvents() {
    return false;
}

void TestPortHost::pollGamepads(std::span<input::GamepadState>) {}

void TestPortHost::rumbleGamepad(std::size_t, float, float, float) {}

void TestPortHost::setGamepadMotion(input::GamepadMotion::Sensors) {}

void TestPortHost::requestWindowClose() {
    runtime.requestClose();
}

void TestPortHost::setWindowFocusable(bool) {}

// The test port has no screen saver, so its screen stays on.
void TestPortHost::setScreenSaverSuspended(bool, std::function<void(Permission::Answer answer)> callback) {
    callback({.state = Permission::State::Granted});
}

// The runtime of the test port plays frames of a fixed length that no display paces.
void TestPortHost::applyFrameRate(int) {}

void TestPortHost::applyMaxPixelDensity(float cap) {
    runtime.setMaxPixelDensity(cap);
}

} // namespace haylen::platform
