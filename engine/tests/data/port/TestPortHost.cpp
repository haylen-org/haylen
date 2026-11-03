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

void TestPortHost::setClipboard(std::string_view value, std::function<void(bool written)> callback) {
    clipboard = value;
    callback(true);
}

std::string TestPortHost::getClipboard() const {
    return clipboard;
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

void TestPortHost::pollGamepads(std::span<input::GamepadState>) {}

void TestPortHost::rumbleGamepad(std::size_t, float, float, float) {}

input::MotionSensors::Support TestPortHost::getMotionSupport() {
    return {.reason = "The test port has no motion sensors."};
}

void TestPortHost::authorizeMotion(std::function<void(Permission::Answer answer)> callback) {
    callback({.state = Permission::State::Granted});
}

void TestPortHost::setMotion(input::MotionSensors::Sensors, float) {}

void TestPortHost::readMotion(input::MotionState&) {}

void TestPortHost::setGamepadMotion(input::MotionSensors::Sensors) {}

void TestPortHost::requestWindowClose() {
    runtime.requestClose();
}

void TestPortHost::setWindowFocusable(bool) {}

void TestPortHost::setScreenSaverSuspended(bool) {}

} // namespace haylen::platform
