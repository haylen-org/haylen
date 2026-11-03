#include "platform/android/AndroidHost.hpp"

#include "platform/NativeWindow.hpp"
#include "platform/Services.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMotion.hpp"
#include "platform/android/AndroidOutputs.hpp"
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

void AndroidHost::setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) {
    callback(JavaBridge::setClipboard(value));
}

std::string AndroidHost::getClipboard() const {
    return JavaBridge::getClipboard();
}

// Android gives the clipboard to the app that has the focus without a prompt, and tells the person with a notice that the app pasted.
void AndroidHost::readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) {
    callback({.state = Permission::State::Granted}, JavaBridge::getClipboard());
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

// The UI thread hands the frame thread its lifecycle and input through a queue that waits for the halt to end, and only the end of the window or the activity holds the UI thread until the frame thread handles it.
bool AndroidHost::pumpHaltedEvents() {
    return runtime.isEnding();
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

void AndroidHost::setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::keepScreenOn(value);
    callback({.state = Permission::State::Granted});
}

// The window of an Android app always covers its screen.
void AndroidHost::setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (!value) {
        callback({.state = Permission::State::Unsupported, .message = "An Android app always covers its screen, so it never leaves fullscreen."});
        return;
    }
    callback({.state = Permission::State::Granted});
}

void AndroidHost::setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (value) {
        callback({.state = Permission::State::Unsupported, .message = "Android apps of the engine read the pointer as it moves on the screen and lock no pointer."});
        return;
    }
    callback({.state = Permission::State::Granted});
}

void AndroidHost::openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) {
    AndroidOutputs::open(number, display, options);
}

void AndroidHost::closeOutput(std::uint32_t number) {
    AndroidOutputs::close(number);
}

AndroidHost::OutputReport AndroidHost::getOutputReport(std::uint32_t number) const {
    return AndroidOutputs::getReport(number, runtime.getGraphics());
}

} // namespace haylen::platform
