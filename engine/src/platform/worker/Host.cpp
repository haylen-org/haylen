#include "platform/worker/Host.hpp"

#include <emscripten/emscripten.h>

#include <optional>
#include <utility>

#include "platform/Player.hpp"
#include "platform/web/BrowserAudioOutput.hpp"
#include "platform/web/WebOutputs.hpp"
#include "platform/web/WebPage.hpp"
#include "platform/worker/Runtime.hpp"

// clang-format off
EM_JS(void, haylen_js_set_title, (const char* title), {
    Module.haylen.setTitle(UTF8ToString(title));
});

EM_JS(void, haylen_js_set_cursor, (const char* cursor), {
    Module.haylen.setCursor(UTF8ToString(cursor));
});

EM_JS(void, haylen_js_rumble_gamepad, (int index, float low, float high, float seconds), {
    Module.haylen.rumbleGamepad(index, low, high, seconds);
});
// clang-format on

namespace haylen::platform::worker {

math::Vec2 Host::getFramebufferSize() const noexcept {
    return runtime.getWindow().getPixelSize();
}

float Host::getDpiScale() const noexcept {
    return runtime.getWindow().getPixelDensity();
}

bool Host::isFullscreen() const noexcept {
    return WebPage::isFullscreen();
}

// The page puts the element that holds the canvas and its overlay in fullscreen, as it does for the runtime on the page.
void Host::setFullscreen(bool value, std::function<void(Permission::Answer answer)> callback) {
    WebPage::setFullscreen(value, getDisplayId(), std::move(callback));
}

void Host::enterFullscreen(const Fullscreen& value, std::function<void(Permission::Answer answer)> callback) {
    const FullscreenTarget target = resolveFullscreen(value);
    WebPage::setFullscreen(true, target.display.id, std::move(callback));
}

void Host::setTitle(std::string_view value) {
    haylen_js_set_title(std::string(value).c_str());
}

void Host::setCursor(Cursor value) {
    cursor = value;

    if (cursorVisible) {
        haylen_js_set_cursor(std::string(toCssCursor(value)).c_str());
    }
}

void Host::setCursorVisible(bool value) {
    cursorVisible = value;
    haylen_js_set_cursor(value ? std::string(toCssCursor(cursor)).c_str() : "none");
}

// The page locks the pointer to the canvas, which browsers allow only during an input of the person, and answers once the browser settled it.
void Host::setMouseLocked(bool value, std::function<void(Permission::Answer answer)> callback) {
    WebPage::lockPointer(value, std::move(callback));
}

void Host::setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) {
    WebPage::setClipboard(value, std::move(callback));
}

std::string Host::getClipboard() const {
    return WebPage::getClipboard();
}

void Host::readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) {
    WebPage::readClipboard(std::move(callback));
}

bool Host::canBeTransparent() const noexcept {
    return runtime.getWindow().isTransparent();
}

std::unique_ptr<gpu::Device> Host::createGpuDevice() {
    return runtime.createDevice();
}

bool Host::prepareGpuDevice() {
    return runtime.prepareDevice();
}

void Host::closeWindow() {
    runtime.close();
}

void Host::endSplashScreen(float fadeOutSeconds) {
    WebPage::endSplashScreen(fadeOutSeconds);
}

// The page plays the mixed audio through its own output, the only backend miniaudio has in browsers.
audio::Mixer::Setup Host::getAudioSetup() const {
    return {.device = true, .backend = &audio::BrowserAudioOutput::kBackend};
}

// The worker delivers every message of the page only once the frame returns, so a halt would never end.
bool Host::canHaltFrames() const noexcept {
    return false;
}

bool Host::pumpHaltedEvents() {
    return false;
}

void Host::pollGamepads(std::span<input::GamepadState> gamepads) {
    runtime.getGamepads().poll(gamepads);
}

void Host::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    const std::optional<std::size_t> browserIndex = runtime.getGamepads().getIndex(index);
    if (browserIndex) {
        haylen_js_rumble_gamepad(static_cast<int>(*browserIndex), low, high, seconds);
    }
}

// The Gamepad API tells pages nothing of the motion sensors of controllers.
void Host::setGamepadMotion(input::GamepadMotion::Sensors) {}

// The page opens a window of the browser on the screen of the output and hands its canvas to the worker, which the WebGPU device presents into.
void Host::openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) {
    if (runtime.getWindow().getApi() != gpu::Device::Api::WebGpu) {
        refuseOutput(number, "A WebGL2 context draws only into its own canvas, so the page shows outputs on other screens only through WebGPU.");
        return;
    }

    WebOutputs::open(number, display, options, runtime.getWindow().getSampleCount());
}

void Host::closeOutput(std::uint32_t number) {
    NativeHost::closeOutput(number);
    WebOutputs::close(number);
}

Host::OutputReport Host::getOutputReport(std::uint32_t number) const {
    const OutputReport refused = NativeHost::getOutputReport(number);
    return refused.reason.empty() ? WebOutputs::getReport(number) : refused;
}

// A page closes no window, so a request to close the window tells the app that it should quit and ends the runtime, as the page of the runtime does.
void Host::requestWindowClose() {
    Player::handleEvent({.type = Event::Type::QuitRequested});
    runtime.close();
}

// The page decides where the focus goes.
void Host::setWindowFocusable(bool) {}

void Host::setScreenSaverSuspended(bool value, std::function<void(Permission::Answer answer)> callback) {
    WebPage::setScreenKeptOn(value, std::move(callback));
}

void Host::applyFrameRate(int framesPerSecond) {
    runtime.setFrameRate(framesPerSecond);
}

void Host::applyMaxPixelDensity(float cap) {
    runtime.setMaxPixelDensity(cap);
}

std::string_view Host::toCssCursor(Cursor value) noexcept {
    switch (value) {
    case Cursor::IBeam:
        return "text";
    case Cursor::Crosshair:
        return "crosshair";
    case Cursor::PointingHand:
        return "pointer";
    case Cursor::ResizeHorizontal:
        return "ew-resize";
    case Cursor::ResizeVertical:
        return "ns-resize";
    case Cursor::ResizeDiagonalDown:
        return "nwse-resize";
    case Cursor::ResizeDiagonalUp:
        return "nesw-resize";
    case Cursor::ResizeAll:
        return "move";
    case Cursor::NotAllowed:
        return "not-allowed";
    default:
        return "default";
    }
}

} // namespace haylen::platform::worker
