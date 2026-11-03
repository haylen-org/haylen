#include "platform/sdl/Host.hpp"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/core/Log.hpp"
#include "platform/sdl/Runtime.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "platform/Services.hpp"
#if defined(__APPLE__) && !TARGET_OS_TV
#include "platform/apple/ApplePasteboard.hpp"
#endif
#if defined(__APPLE__) && !TARGET_OS_OSX
#include "platform/apple/AppleGamepads.hpp"
#include "platform/apple/AppleMotion.hpp"
#include "platform/apple/AppleOutputs.hpp"
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebMotion.hpp"
#include "platform/web/WebOutputs.hpp"
#include "platform/web/WebPage.hpp"
#endif

namespace haylen::platform::sdl {

Host::~Host() {
    for (SDL_Cursor* cursor : cursors) {
        SDL_DestroyCursor(cursor);
    }
}

math::Vec2 Host::getFramebufferSize() const noexcept {
    return runtime.getWindow().getPixelSize();
}

// The scale is the framebuffer pixels per point, which SDL measures through the pixels per unit of the window and the scale of the display where window units are pixels, as on Windows and X11. A window that opened without high DPI keeps one framebuffer pixel per unit of the window.
float Host::getDpiScale() const noexcept {
    SDL_Window* window = runtime.getWindow().get();
    return runtime.isHighDpi() ? SDL_GetWindowDisplayScale(window) : SDL_GetWindowPixelDensity(window);
}

bool Host::isFullscreen() const noexcept {
    return (SDL_GetWindowFlags(runtime.getWindow().get()) & SDL_WINDOW_FULLSCREEN) != 0U;
}

// Desktops open a window of SDL on the display of the output, iPhones and iPads show it in the scene of an external display, a page opens a window of the browser on the screen of the output, and the other platforms end it with the reason.
void Host::openOutput(std::uint32_t number, std::string_view display, const Output::Options& options) {
#if defined(__EMSCRIPTEN__)
    if (runtime.getWindow().getApi() != gpu::Device::Api::WebGpu) {
        refuseOutput(number, "A WebGL2 context draws only into its own canvas, so the page shows outputs on other screens only through WebGPU.");
        return;
    }
    WebOutputs::open(number, display, options, runtime.getWindow().getSampleCount());
#elif defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleOutputs::open(number, display, {.sampleCount = runtime.getWindow().getSampleCount(), .vsync = runtime.getWindow().isVsync()});
#elif defined(__APPLE__) && !TARGET_OS_OSX
    refuseOutput(number, "This platform shows the app on the display of its window alone.");
#else
    const std::vector<Display> displays = getDisplays();
    const auto found = std::ranges::find(displays, display, &Display::id);
    if (found == displays.end()) {
        refuseOutput(number, "The display \"" + std::string(display) + "\" disconnected before its output opened.");
        return;
    }
    try {
        runtime.getOutputs().open(number, *found, options, runtime.getWindow());
    } catch (const std::runtime_error& error) {
        refuseOutput(number, error.what());
    }
#endif
}

void Host::closeOutput(std::uint32_t number) {
    NativeHost::closeOutput(number);
#if defined(__EMSCRIPTEN__)
    WebOutputs::close(number);
#elif defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleOutputs::close(number);
#elif !defined(__APPLE__) || TARGET_OS_OSX
    runtime.getOutputs().close(number);
#endif
}

Host::OutputReport Host::getOutputReport(std::uint32_t number) const {
#if defined(__EMSCRIPTEN__)
    const OutputReport refused = NativeHost::getOutputReport(number);
    return refused.reason.empty() ? WebOutputs::getReport(number) : refused;
#elif defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    return AppleOutputs::getReport(number);
#elif !defined(__APPLE__) || TARGET_OS_OSX
    const OutputReport refused = NativeHost::getOutputReport(number);
    return refused.reason.empty() ? runtime.getOutputs().getReport(number) : refused;
#else
    return NativeHost::getOutputReport(number);
#endif
}

// A desktop window goes fullscreen on the display that shows it, in the mode that the desktop runs in.
void Host::setFullscreen(bool value) {
    if (value && Services::hasDesktop()) {
        enterFullscreen({});
        return;
    }
    if (isFullscreen() == value) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    WebPage::setFullscreen(value, getDisplayId());
#else
    SDL_Window* window = runtime.getWindow().get();
    SDL_SetWindowFullscreen(window, value);
    // Leaving fullscreen restores the mode of the desktop and the default style of the window, which may have lost its decorations or its fixed size.
    if (!value) {
        SDL_SetWindowFullscreenMode(window, nullptr);
        applyStyle();
    }
#endif
}

// The page puts the element that holds the canvas and its overlay in fullscreen, so the dialogs, the fields and the views of plugins stay over the app, on the screen that the Fullscreen API takes. Desktops move the window, and the other platforms keep it on its display.
void Host::enterFullscreen(const Fullscreen& value) {
#if defined(__EMSCRIPTEN__)
    const FullscreenTarget target = resolveFullscreen(value);
    WebPage::setFullscreen(true, target.display.id);
#else
    if (!Services::hasDesktop()) {
        NativeHost::enterFullscreen(value);
        return;
    }
    enterDesktopFullscreen(resolveFullscreen(value));
#endif
}

// The modes of SDL count their size in the units of the desktop, whose pixels are the size times the density.
SDL_DisplayMode Host::findMode(SDL_DisplayID display, const Display::Mode& mode) {
    constexpr float kRateTolerance = 0.5F;
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    SDL_DisplayMode found{};
    for (int index = 0; index < count && found.displayID == 0; ++index) {
        const SDL_DisplayMode& candidate = *modes[index];
        const bool size = std::lround(static_cast<float>(candidate.w) * candidate.pixel_density) == mode.width && std::lround(static_cast<float>(candidate.h) * candidate.pixel_density) == mode.height;
        if (size && std::abs(candidate.refresh_rate - mode.refreshRate) <= kRateTolerance) {
            found = candidate;
        }
    }
    SDL_free(static_cast<void*>(modes));
    if (found.displayID == 0) {
        throw std::runtime_error(std::format("SDL lists no fullscreen mode of {} by {} pixels at {} frames per second.", mode.width, mode.height, mode.refreshRate));
    }
    return found;
}

// A fullscreen window leaves fullscreen before it moves, and the move ends before the window covers the display, so the fullscreen lands on the display that the app chose.
void Host::enterDesktopFullscreen(const FullscreenTarget& target) {
    SDL_Window* window = runtime.getWindow().get();
    const SDL_DisplayID display = sdl::Window::findDisplay(target.display);
    const std::optional<SDL_DisplayMode> exclusive = target.mode ? std::optional(findMode(display, *target.mode)) : std::nullopt;
    const SDL_DisplayMode* current = SDL_GetWindowFullscreenMode(window);
    const bool sameMode = exclusive ? current != nullptr && current->w == exclusive->w && current->h == exclusive->h && current->refresh_rate == exclusive->refresh_rate && current->pixel_density == exclusive->pixel_density : current == nullptr;
    if (isFullscreen() && sameMode && SDL_GetDisplayForWindow(window) == display) {
        return;
    }
    if (isFullscreen()) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SyncWindow(window);
    }
    if (SDL_GetDisplayForWindow(window) != display) {
        SDL_SetWindowPosition(window, static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display)), static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display)));
        SDL_SyncWindow(window);
    }
    if (!SDL_SetWindowFullscreenMode(window, exclusive ? &*exclusive : nullptr) || !SDL_SetWindowFullscreen(window, true)) {
        throw std::runtime_error("The window could not go fullscreen on the display \"" + target.display.id + "\". " + SDL_GetError());
    }
}

void Host::setTitle(std::string_view value) {
    SDL_SetWindowTitle(runtime.getWindow().get(), std::string(value).c_str());
}

void Host::setCursor(Cursor value) {
    if (value == Cursor::Default) {
        SDL_SetCursor(SDL_GetDefaultCursor());
        return;
    }
    SDL_Cursor*& cursor = cursors[static_cast<std::size_t>(value)];
    if (cursor == nullptr) {
        cursor = SDL_CreateSystemCursor(toSystemCursor(value));
    }
    SDL_SetCursor(cursor);
}

void Host::setCursorVisible(bool value) {
    if (value) {
        SDL_ShowCursor();
        return;
    }
    SDL_HideCursor();
}

void Host::setMouseLocked(bool value) {
    SDL_SetWindowRelativeMouseMode(runtime.getWindow().get(), value);
}

// SDL has no clipboard of the browser, so the page writes it and keeps what it may read.
void Host::setClipboard(std::string_view value, std::function<void(Permission::Answer answer)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::setClipboard(value, std::move(callback));
#else
    if (!SDL_SetClipboardText(std::string(value).c_str())) {
        callback({.state = Permission::State::Unsupported, .message = std::format("The clipboard could not be written: {}", SDL_GetError())});
        return;
    }
    callback({.state = Permission::State::Granted});
#endif
}

std::string Host::getClipboard() const {
#if defined(__EMSCRIPTEN__)
    return WebPage::getClipboard();
#else
    char* text = SDL_GetClipboardText();
    std::string result = text != nullptr ? text : "";
    SDL_free(text);
    return result;
#endif
}

// The pasteboards of Apple platforms and the clipboard of the browser may ask the person before the app reads them, while Windows, Linux and the clipboard that an Apple TV keeps inside the app answer during the call.
void Host::readClipboard(std::function<void(Permission::Answer answer, std::string text)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::readClipboard(std::move(callback));
#elif defined(__APPLE__) && !TARGET_OS_TV
    ApplePasteboard::read(std::move(callback));
#else
    callback({.state = Permission::State::Granted}, getClipboard());
#endif
}

// The window composes premultiplied alpha with the desktop when it opened transparent, and while it is opaque the frames keep alpha 1.
bool Host::canBeTransparent() const noexcept {
    return runtime.isTransparent();
}

// Apple platforms and the web edit text in a native field of the services, while Windows and Linux type through the text events of SDL.
platform::TextInput& Host::getTextInput() noexcept {
#if defined(_WIN32) || defined(__linux__)
    return runtime.getTextInput();
#else
    return Services::getTextInput();
#endif
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

// A web page runs the app on the thread of the browser that delivers every message, which only goes on once the frame returns.
bool Host::canHaltFrames() const noexcept {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return true;
#endif
}

bool Host::pumpHaltedEvents() {
    return runtime.pumpHaltedEvents();
}

// UIKit apps read their controllers through GameController, which reads the Siri Remote as a controller of its own while UIKit keeps its presses for the focus, and SDL reads them through GameController there too, so it would add nothing.
void Host::pollGamepads(std::span<input::GamepadState> gamepads) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::poll(gamepads);
#else
    runtime.getGamepads().poll(gamepads);
#endif
}

void Host::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::rumble(index, low, high, seconds);
#else
    runtime.getGamepads().rumble(index, low, high, seconds);
#endif
}

// Phones, tablets and TVs of Apple read the motion of the device through Core Motion, browsers through the motion events of the page, and computers through the sensors of SDL.
input::MotionSensors::Support Host::getMotionSupport() {
#if defined(__APPLE__) && !TARGET_OS_OSX
    return AppleMotion::getSupport();
#elif defined(__EMSCRIPTEN__)
    return WebMotion::getSupport();
#else
    return sensors.getSupport();
#endif
}

// Browsers and SDL read each sensor at the rate of its hardware.
void Host::setMotion(input::MotionSensors::Sensors value, [[maybe_unused]] float rate) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleMotion::set(value, rate);
#elif defined(__EMSCRIPTEN__)
    WebMotion::set(value);
#else
    sensors.set(value);
#endif
}

void Host::readMotion(input::MotionState& state) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleMotion::read(state);
#elif defined(__EMSCRIPTEN__)
    WebMotion::read(state);
#else
    sensors.read(state, runtime.getWindow().get());
#endif
}

void Host::setGamepadMotion(input::MotionSensors::Sensors value) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::setMotion(value);
#else
    runtime.getGamepads().setMotion(value);
#endif
}

void Host::requestWindowClose() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

void Host::setWindowFocusable(bool value) {
    SDL_SetWindowFocusable(runtime.getWindow().get(), value);
}

// SDL keeps the screen on through the idle timer of UIKit, the power assertions of macOS, the execution state of Windows and the inhibitors of Linux desktops, and the page asks the browser for a wake lock.
void Host::setScreenSaverSuspended(bool value) {
#if defined(__EMSCRIPTEN__)
    WebPage::setScreenKeptOn(value);
#else
    if (value) {
        SDL_DisableScreenSaver();
    } else {
        SDL_EnableScreenSaver();
    }
#endif
}

SDL_SystemCursor Host::toSystemCursor(Cursor cursor) noexcept {
    switch (cursor) {
    case Cursor::IBeam:
        return SDL_SYSTEM_CURSOR_TEXT;
    case Cursor::Crosshair:
        return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case Cursor::PointingHand:
        return SDL_SYSTEM_CURSOR_POINTER;
    case Cursor::ResizeHorizontal:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case Cursor::ResizeVertical:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case Cursor::ResizeDiagonalDown:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case Cursor::ResizeDiagonalUp:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case Cursor::ResizeAll:
        return SDL_SYSTEM_CURSOR_MOVE;
    case Cursor::NotAllowed:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

} // namespace haylen::platform::sdl
