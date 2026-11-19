#include "platform/android/AndroidHost.hpp"

#include "haylen/core/Engine.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidOutputs.hpp"
#include "platform/android/AndroidRuntime.hpp"
#include "platform/android/AndroidTextInput.hpp"
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

// Native events wait in Java while no app runs, so the ones that come while an app stops reach the next app instead of none. The app tells whether it takes back as soon as it started, before its first frame.
void AndroidHost::appStarted(core::Engine& engine, bool) {
    JavaBridge::setAppRunning(true);
    reportBack(engine);
}

void AndroidHost::appStopping() {
    JavaBridge::setAppRunning(false);
}

void AndroidHost::framePlayed(core::Engine& engine) {
    reportBack(engine);
}

// Back reaches the app only through the back callback of the activity, which is enabled only while the app takes back or edits a text field, so the predictive back animation of the system plays only when back leaves the app. The callback of a new activity holds back until the first report.
void AndroidHost::reportBack(core::Engine& engine) {
    const bool captured = engine.isBackCaptured() || AndroidTextInput::isEditing() || isLaunched() || isBackHeld();
    if (captured == backReported) {
        return;
    }

    backReported = captured;
    JavaBridge::captureBack(captured);
}

// The on-screen keyboard of Android arrives from the UI thread in window pixels.
void AndroidHost::preparePostedEvent(Event& event) const {
    if (event.type == Event::Type::KeyboardChanged) {
        event.keyboardFrame = AndroidActivity::toFramebuffer(event.keyboardFrame);
    }
}

void AndroidHost::closeWindow() {
    runtime.quit();
}

void AndroidHost::endSplashScreen(float fadeOutSeconds) {
    AndroidActivity::endSplashScreen(fadeOutSeconds);
}

audio::Mixer::Setup AndroidHost::getAudioSetup() const {
    return {.device = true};
}

// The views of Android plugins reserve edges in window pixels.
math::Insets AndroidHost::getReservedInsets() const {
    return AndroidActivity::toFramebuffer(getNativeViews().getReservedInsets());
}

// The UI thread hands the frame thread its lifecycle and input through a queue that waits for the halt to end, and only the end of the window or the activity holds the UI thread until the frame thread handles it.
bool AndroidHost::pumpHaltedEvents() {
    return runtime.isEnding();
}

void AndroidHost::pollGamepads(std::span<input::GamepadState> gamepads) {
    AndroidGamepads::poll(gamepads);
}

void AndroidHost::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    AndroidGamepads::rumble(index, low, high, seconds);
}

// The engine reads the sticks and buttons of controllers on Android from their input events, which carry none of their motion sensors.
void AndroidHost::setGamepadMotion(input::GamepadMotion::Sensors) {}

void AndroidHost::requestWindowClose() {
    runtime.requestQuit();
}

void AndroidHost::setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) {
    JavaBridge::keepScreenOn(value);
    callback({.state = Permission::State::Granted});
}

// The window of an Android app always covers its screen.
void AndroidHost::applyFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (!value) {
        callback({.state = Permission::State::Unsupported, .message = "An Android app always covers its screen, so it never leaves fullscreen."});
        return;
    }

    callback({.state = Permission::State::Granted});
}

void AndroidHost::applyMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) {
    if (value) {
        callback({.state = Permission::State::Unsupported, .message = "Android apps of the engine read the pointer as it moves on the screen and lock no pointer."});
        return;
    }

    callback({.state = Permission::State::Granted});
}

void AndroidHost::applyFrameRate(int framesPerSecond) {
    runtime.setFrameRate(framesPerSecond);
}

void AndroidHost::applyMaxPixelDensity(float cap) {
    runtime.setMaxPixelDensity(cap);
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
