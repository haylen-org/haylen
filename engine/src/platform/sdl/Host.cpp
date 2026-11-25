#include "platform/sdl/Host.hpp"

#include <SDL3/SDL_clipboard.h>
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
#include "platform/TextHelper.hpp"
#include "platform/sdl/Runtime.hpp"
#include "platform/sdl/WaylandDisplays.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "platform/Services.hpp"
#if defined(__APPLE__) && !TARGET_OS_TV
#include "platform/apple/ApplePasteboard.hpp"
#endif
#if defined(__APPLE__)
#include "platform/apple/AppleRuntime.hpp"
#include "platform/apple/AppleTextInput.hpp"
#endif
#if defined(__APPLE__) && TARGET_OS_IOS
#include "platform/apple/ApplePointer.hpp"
#endif
#if defined(__APPLE__) && !TARGET_OS_OSX
#include "platform/apple/AppleGamepads.hpp"
#include "platform/apple/AppleOutputs.hpp"
#include "platform/apple/AppleSplash.hpp"
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebOutputs.hpp"
#include "platform/web/WebPage.hpp"
#include "platform/web/WebTextInput.hpp"
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

// The scale is the framebuffer pixels per point, which SDL measures through the pixels per unit of the window and the scale of the display where window units are pixels, as on Windows and X11, and which the pixel density cap lowers.
float Host::getDpiScale() const noexcept {
    return runtime.getWindow().getDpiScale();
}

bool Host::isFullscreen() const noexcept {
#if defined(__EMSCRIPTEN__)
    return WebPage::isFullscreen();
#else
    return (SDL_GetWindowFlags(runtime.getWindow().get()) & SDL_WINDOW_FULLSCREEN) != 0U;
#endif
}

// Desktops open a window of SDL on the display of the output, iPhones and iPads show it in the scene of an external display, a page opens a window of the browser on the screen of the output, and the other platforms end it with the reason.
void Host::openOutput(std::uint32_t number, [[maybe_unused]] std::string_view display, [[maybe_unused]] const Output::Options& options) {
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

void Host::setExternalDisplays([[maybe_unused]] bool value) {
#if defined(__APPLE__) && TARGET_OS_IOS && !TARGET_OS_MACCATALYST
    AppleOutputs::setAccessory(value);
#endif
}

// A desktop window goes fullscreen on the display that shows it, in the mode that the desktop runs in. The page answers once the browser settled the change. The window of SDL answers once SDL reports the mode it asked for: during the call where SDL changes the mode at once, as on Windows or for a hidden window, and at the end of the animation where the system animates the window into it, as on macOS. SDL never reports a change that the system reverts before it ends, such as one that a newer change replaces on macOS, so the mode that SDL reports right after the call is the one the window keeps.
void Host::applyFullscreen(bool value, std::function<void(SystemAnswer answer)> callback) {
    if (value && Services::hasDesktop()) {
        enterFullscreen({}, std::move(callback));
        return;
    }
#if defined(__EMSCRIPTEN__)
    WebPage::setFullscreen(value, getDisplayId(), std::move(callback));
#else
    if (!fullscreenRequest.isPending() && isFullscreen() == value) {
        callback({.state = SystemAnswer::State::Granted});
        return;
    }

    fullscreenRequest.begin(value, std::move(callback));

    if (!SDL_SetWindowFullscreen(runtime.getWindow().get(), value)) {
        fullscreenRequest.fail({.state = SystemAnswer::State::Unsupported, .message = std::format("The window could not {} fullscreen. {}", value ? "go" : "leave", SDL_GetError())});
        return;
    }

    fullscreenRequest.reach(isFullscreen());
#endif
}

// Leaving fullscreen forgets the exclusive mode, so a fullscreen that the system starts covers the display at the mode of the desktop, and restores the style of the window, which may have lost its decorations or its fixed size. A window that leaves only on its way to fullscreen on another display keeps both, and covers that display once it left.
void Host::followFullscreen(bool entered) {
    if (!entered && fullscreenRequest.isLeaving()) {
        coverDisplay(fullscreenRequest.takeDestination());
        return;
    }

    if (!entered && !fullscreenRequest.isEntering()) {
        SDL_SetWindowFullscreenMode(runtime.getWindow().get(), nullptr);
        applyStyle();
    }

    fullscreenRequest.reach(entered);
}

// The page puts the element that holds the canvas and its overlay in fullscreen, so the fields and the views of plugins stay over the app, on the screen that the Fullscreen API takes. Desktops move the window, and the other platforms keep it on its display.
void Host::applyFullscreenTarget(const Fullscreen& value, std::function<void(SystemAnswer answer)> callback) {
#if defined(__EMSCRIPTEN__)
    const FullscreenTarget target = resolveFullscreen(value);
    WebPage::setFullscreen(true, target.display.id, std::move(callback));
#else
    if (!Services::hasDesktop()) {
        NativeHost::applyFullscreenTarget(value, std::move(callback));
        return;
    }

    enterDesktopFullscreen(resolveFullscreen(value), std::move(callback));
#endif
}

// The modes of SDL count their size in the units of the desktop, whose pixels are the size times the density.
std::optional<SDL_DisplayMode> Host::findMode(SDL_DisplayID display, const Display::Mode& mode) {
    constexpr float kRateTolerance = 0.5F;
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    std::optional<SDL_DisplayMode> found;
    for (int index = 0; index < count && !found; ++index) {
        const SDL_DisplayMode& candidate = *modes[index];
        const Display::Mode described = sdl::Window::describeMode(candidate);
        const bool size = described.width == mode.width && described.height == mode.height;
        if (size && std::abs(candidate.refresh_rate - mode.refreshRate) <= kRateTolerance) {
            found = candidate;
        }
    }

    SDL_free(static_cast<void*>(modes));
    return found;
}

// A fullscreen window leaves fullscreen before it moves, and covers the display that the app chose once SDL reports that it left, so the frame thread never waits for the animation of the system. A hidden window takes its fullscreen only when it shows, so it moves at once.
void Host::enterDesktopFullscreen(const FullscreenTarget& target, std::function<void(SystemAnswer answer)> callback) {
    SDL_Window* window = runtime.getWindow().get();
    const SDL_DisplayID display = sdl::Window::findDisplay(target.display);
    const std::optional<SDL_DisplayMode> exclusive = target.mode ? findMode(display, *target.mode) : std::nullopt;
    if (target.mode && !exclusive) {
        callback({.state = SystemAnswer::State::Unsupported, .message = std::format("SDL lists no fullscreen mode of {} by {} pixels at {} frames per second.", target.mode->width, target.mode->height, target.mode->refreshRate)});
        return;
    }

    const SDL_DisplayMode* current = SDL_GetWindowFullscreenMode(window);
    const bool sameMode = exclusive ? current != nullptr && current->w == exclusive->w && current->h == exclusive->h && current->refresh_rate == exclusive->refresh_rate && current->pixel_density == exclusive->pixel_density : current == nullptr;
    if (!fullscreenRequest.isPending() && isFullscreen() && sameMode && SDL_GetDisplayForWindow(window) == display) {
        callback({.state = SystemAnswer::State::Granted});
        return;
    }

    FullscreenRequest::Destination destination{.display = display, .mode = exclusive, .displayId = target.display.id};
    const bool hidden = (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0U;
    if (!isFullscreen() || hidden) {
        fullscreenRequest.begin(true, std::move(callback));
        coverDisplay(destination);
        return;
    }

    fullscreenRequest.beginLeaving(std::move(destination), std::move(callback));

    if (!SDL_SetWindowFullscreen(window, false)) {
        fullscreenRequest.fail({.state = SystemAnswer::State::Unsupported, .message = "The window could not leave fullscreen to cover the display \"" + target.display.id + "\". " + SDL_GetError()});
    }
}

// SDL keeps the display of a move that the system has not finished for the fullscreen that follows it, so the window covers the display it moves to.
void Host::coverDisplay(const FullscreenRequest::Destination& destination) {
    SDL_Window* window = runtime.getWindow().get();
    if (SDL_GetDisplayForWindow(window) != destination.display) {
        const int centered = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(destination.display));
        SDL_SetWindowPosition(window, centered, centered);
    }

    const SDL_DisplayMode* mode = destination.mode ? &*destination.mode : nullptr;
    if (!SDL_SetWindowFullscreenMode(window, mode) || !SDL_SetWindowFullscreen(window, true)) {
        fullscreenRequest.fail({.state = SystemAnswer::State::Unsupported, .message = "The window could not go fullscreen on the display \"" + destination.displayId + "\". " + SDL_GetError()});
        return;
    }

    fullscreenRequest.reach(isFullscreen());
}

// The windows of the systems take only UTF-8 titles, and the one of macOS ends the app on any other.
void Host::setTitle(std::string_view value) {
    SDL_SetWindowTitle(runtime.getWindow().get(), TextHelper::toValidUtf8(value).c_str());
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

// A mode that the system may refuse, the lock of a browser or of an iPad, keeps the mode before it unless the system grants it, so the pointer never locks later for a mode that the app no longer has.
void Host::applyMouseMode(MouseMode value, std::function<void(SystemAnswer answer)> callback) {
    if (const std::optional<std::string> refusal = refuseMouseMode(value)) {
        callback({.state = SystemAnswer::State::Unsupported, .message = *refusal});
        return;
    }

    const MouseMode previous = appliedMouseMode;
    if (!applySdlMouseMode(value)) {
        const std::string error = SDL_GetError();
        if (!applySdlMouseMode(previous)) {
            core::Log::error("The mouse could not go back to the mode \"{}\". {}", mouseModeName(previous), SDL_GetError());
        }

        callback({.state = SystemAnswer::State::Unsupported, .message = std::format("The mouse could not take the mode \"{}\". {}", mouseModeName(value), error)});
        return;
    }

    if (value != MouseMode::Locked) {
        callback({.state = SystemAnswer::State::Granted});
        return;
    }

    const std::uint64_t change = mouseModeChange;

    // clang-format off
    awaitMouseLock([this, previous, change, callback = std::move(callback)](SystemAnswer answer) {
        if (answer.state != SystemAnswer::State::Granted && change == mouseModeChange && !applySdlMouseMode(previous)) {
            core::Log::error("The mouse could not go back to the mode \"{}\". {}", mouseModeName(previous), SDL_GetError());
        }

        callback(std::move(answer));
    });
    // clang-format on
}

// Browsers keep no pointer inside a page, iPads and Macs keep none inside the window of an iPad app, and iPhones, TVs and headsets show no pointer that an app controls.
std::optional<std::string> Host::refuseMouseMode([[maybe_unused]] MouseMode value) {
#if defined(__EMSCRIPTEN__)
    return WebPage::refuseMouseMode(value);
#elif defined(__APPLE__) && TARGET_OS_IOS
    if (value != MouseMode::Normal && !ApplePointer::isControllable()) {
        return std::format("An iPhone has no pointer that an app controls, so the mouse cannot take the mode \"{}\".", mouseModeName(value));
    }

    if (value == MouseMode::Confined || value == MouseMode::ConfinedHidden) {
        return std::format("An iPad or a Mac keeps no pointer inside the window of an iPad app, so the mouse cannot take the mode \"{}\".", mouseModeName(value));
    }

    return std::nullopt;
#elif defined(__APPLE__) && !TARGET_OS_OSX
    if (value != MouseMode::Normal) {
        return std::format("An Apple TV and an Apple Vision Pro have no pointer that an app controls, so the mouse cannot take the mode \"{}\".", mouseModeName(value));
    }

    return std::nullopt;
#else
    return std::nullopt;
#endif
}

// SDL holds the pointer of a locked or confined window only while the window has the focus, and holds it again once the window gets the focus back.
bool Host::applySdlMouseMode(MouseMode value) {
    SDL_Window* window = runtime.getWindow().get();
    const bool locked = value == MouseMode::Locked;
    const bool confined = value == MouseMode::Confined || value == MouseMode::ConfinedHidden;
    const bool visible = value == MouseMode::Normal || value == MouseMode::Confined;
    if (!SDL_SetWindowRelativeMouseMode(window, locked) || !SDL_SetWindowMouseGrab(window, confined) || !(visible ? SDL_ShowCursor() : SDL_HideCursor())) {
        return false;
    }
#if defined(__APPLE__) && TARGET_OS_IOS
    ApplePointer::refreshStyle();
#endif
    appliedMouseMode = value;
    ++mouseModeChange;
    return true;
}

// A desktop locks the pointer at once, while a browser locks it during an input of the person and UIKit where it lets the scene of the app keep it, which they answer.
void Host::awaitMouseLock(std::function<void(SystemAnswer answer)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::awaitPointerLock(true, std::move(callback));
#elif defined(__APPLE__) && TARGET_OS_IOS
    ApplePointer::awaitLock(std::move(callback));
#else
    callback({.state = SystemAnswer::State::Granted});
#endif
}

// SDL has no clipboard of the browser, so the page writes it and keeps what it may read.
void Host::setClipboard(std::string_view value, std::function<void(SystemAnswer answer)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::setClipboard(value, std::move(callback));
#else
    if (!SDL_SetClipboardText(TextHelper::toValidUtf8(value).c_str())) {
        callback({.state = SystemAnswer::State::Unsupported, .message = std::format("The clipboard could not be written: {}", SDL_GetError())});
        return;
    }

    callback({.state = SystemAnswer::State::Granted});
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
void Host::readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::readClipboard(std::move(callback));
#elif defined(__APPLE__) && !TARGET_OS_TV
    ApplePasteboard::read(std::move(callback));
#else
    callback({.state = SystemAnswer::State::Granted}, getClipboard());
#endif
}

// The window composes premultiplied alpha with the desktop when it opened transparent, and while it is opaque the frames keep alpha 1.
bool Host::canBeTransparent() const noexcept {
    return runtime.isTransparent();
}

platform::TextInput& Host::getPlatformTextInput([[maybe_unused]] Runtime& owner) noexcept {
#if defined(__APPLE__)
    return AppleTextInput::getShared();
#elif defined(__EMSCRIPTEN__)
    return WebTextInput::getShared();
#else
    return owner.getTextInput();
#endif
}

// The apps of iPhones, iPads, TVs and pages never end themselves.
bool Host::allowsQuit() const noexcept {
#if defined(__EMSCRIPTEN__) || (defined(__APPLE__) && !TARGET_OS_OSX)
    return false;
#else
    return true;
#endif
}

// The mixer plays through the audio output that the platform registered, such as the output of the page in browsers, and through the output of the system that the audio library of the engine knows everywhere else.
audio::Mixer::Setup Host::getAudioSetup() const {
    return {.device = true, .output = Services::getAudioOutput()};
}

std::unique_ptr<gpu::Device> Host::createGpuDevice() {
    return runtime.createDevice();
}

bool Host::prepareGpuDevice() {
    return runtime.prepareDevice();
}

// The apps of Apple platforms hear the events of their native plugins only while an app runs, and a page hears when an app starts, fails to start and stops.
void Host::appStarted([[maybe_unused]] core::Engine& engine, [[maybe_unused]] bool playing) {
#if defined(__APPLE__)
    AppleRuntime::setAppRunning(true);
#elif defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStarted(engine);
    }
#endif
}

void Host::appStartFailed([[maybe_unused]] const core::Json& report) {
#if defined(__EMSCRIPTEN__)
    WebPage::reportStartFailed(report);
#endif
}

void Host::appStopping() {
#if defined(__APPLE__)
    AppleRuntime::setAppRunning(false);
#endif
}

void Host::appStopped([[maybe_unused]] bool playing) {
#if defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStopped();
    }
#endif
}

// A page shows the statistics of each frame and applies the files it edited between frames.
void Host::frameFinished([[maybe_unused]] core::Engine& engine) {
#if defined(__EMSCRIPTEN__)
    WebPage::reportFrame(engine);
    WebPage::finishFrame();
#endif
}

// A page hears what every batch of a development server did, as the web editor shows it.
void Host::developmentEnabled([[maybe_unused]] DevelopmentSession& session) {
#if defined(__EMSCRIPTEN__)
    session.addReporter(&WebPage::reportReloaded);
#endif
}

// A page shows an app that cannot go on on its error screen, and edits every package that it plays file by file.
bool Host::keepsWindowAfterFailure() const noexcept {
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}

bool Host::overlaysEveryPackage() const noexcept {
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}

void Host::closeWindow() {
    runtime.close();
}

// A Wayland session keeps its outputs to the compositor, where SDL reads them, while the services read the displays of every other desktop with the ids and work areas of its own system.
std::vector<Display> Host::getDisplays() const {
    if (WaylandDisplays::isSession()) {
        return WaylandDisplays::read();
    }

    return Services::getDisplays();
}

std::string Host::getDisplayId() const {
    if (WaylandDisplays::isSession()) {
        return WaylandDisplays::findWindowDisplay(runtime.getWindow().get());
    }

    return NativeHost::getDisplayId();
}

// A Mac, Windows and Linux open their window at once, without a launch screen, while the other Apple platforms keep theirs over the window and a page shows its own splash.
void Host::endSplashScreen([[maybe_unused]] float fadeOutSeconds) {
    runtime.showWhenComplete();
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleSplash::end(fadeOutSeconds);
#elif defined(__EMSCRIPTEN__)
    WebPage::endSplashScreen(fadeOutSeconds);
#endif
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

void Host::setGamepadMotion(input::GamepadMotion::Sensors value) {
#if defined(__APPLE__) && !TARGET_OS_OSX
    AppleGamepads::setMotion(value);
#else
    runtime.getGamepads().setMotion(value);
#endif
}

void Host::requestWindowClose() {
    runtime.requestQuit();
}

void Host::setWindowFocusable(bool value) {
    SDL_SetWindowFocusable(runtime.getWindow().get(), value);
}

void Host::applyFrameRate(int framesPerSecond) {
    runtime.setFrameRate(framesPerSecond);
}

void Host::applyMaxPixelDensity(float cap) {
    runtime.setMaxPixelDensity(cap);
}

// SDL keeps the screen on through the idle timer of UIKit, the power assertions of macOS, the execution state of Windows and the inhibitors of Linux desktops, and the page asks the browser for a wake lock.
void Host::setScreenSaverSuspended(bool value, std::function<void(SystemAnswer answer)> callback) {
#if defined(__EMSCRIPTEN__)
    WebPage::setScreenKeptOn(value, std::move(callback));
#else
    if (!(value ? SDL_DisableScreenSaver() : SDL_EnableScreenSaver())) {
        callback({.state = SystemAnswer::State::Unsupported, .message = std::format("The screen saver could not be {}. {}", value ? "kept away" : "let back", SDL_GetError())});
        return;
    }

    callback({.state = SystemAnswer::State::Granted});
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
