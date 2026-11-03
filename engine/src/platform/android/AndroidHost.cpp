#include "platform/android/AndroidHost.hpp"

#include "platform/NativeWindow.hpp"
#include "platform/Services.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMotion.hpp"
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

void AndroidHost::setClipboard(std::string_view value, std::function<void(bool written)> callback) {
    callback(JavaBridge::setClipboard(value));
}

std::string AndroidHost::getClipboard() const {
    return JavaBridge::getClipboard();
}

// The devices of Vulkan and OpenGL ES start during the call that creates them.
bool AndroidHost::prepareGpuDevice() {
    return true;
}

std::unique_ptr<gpu::Device> AndroidHost::createGpuDevice() {
    return runtime.getGraphics().createDevice();
}

void AndroidHost::closeWindow() {
    runtime.quit();
}

TextInput& AndroidHost::getTextInput() noexcept {
    return Services::getTextInput();
}

void AndroidHost::pollGamepads(std::span<input::GamepadState> gamepads) {
    AndroidGamepads::poll(gamepads);
}

void AndroidHost::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    AndroidGamepads::rumble(index, low, high, seconds);
}

input::MotionSensors::Support AndroidHost::getMotionSupport() {
    return AndroidMotion::getSupport();
}

// Android asks the person for none of the motion sensors that the engine reads.
void AndroidHost::authorizeMotion(std::function<void(Permission::Answer answer)> callback) {
    callback({.state = Permission::State::Granted});
}

void AndroidHost::setMotion(input::MotionSensors::Sensors sensors, float rate) {
    AndroidMotion::set(sensors, rate);
}

void AndroidHost::readMotion(input::MotionState& state) {
    AndroidMotion::read(state);
}

// The engine reads the sticks and buttons of controllers on Android from their input events, which carry none of their motion sensors.
void AndroidHost::setGamepadMotion(input::MotionSensors::Sensors) {}

void AndroidHost::requestWindowClose() {
    runtime.requestQuit();
}

void AndroidHost::setScreenSaverSuspended(bool value) {
    JavaBridge::keepScreenOn(value);
}

} // namespace haylen::platform
