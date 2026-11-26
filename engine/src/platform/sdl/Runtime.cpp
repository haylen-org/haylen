#include "platform/sdl/Runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

// The runtime holds the entry points itself and enters the main callbacks of SDL through the functions they name.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_touch.h>
#include <SDL3/SDL_video.h>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Log.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/Player.hpp"
#include "platform/ProcessorCheck.hpp"
#include "platform/Services.hpp"
#include "platform/sdl/Hints.hpp"
#include "platform/sdl/Host.hpp"
#include "platform/sdl/WaylandDisplays.hpp"
#include "platform/web/PageModifiers.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>

#include "haylen/platform/apple/HaylenMain.h"
#include "platform/apple/AppleServices.hpp"
#include "platform/sdl/apple/AppleApplication.hpp"
#include "platform/sdl/apple/AppleFramePacer.hpp"
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

#include "platform/web/WebEntry.hpp"
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>

#include "platform/windows/WindowsServices.hpp"
#else
#include "platform/linux/LinuxAccessibility.hpp"
#include "platform/linux/LinuxServices.hpp"
#endif

namespace haylen::platform::sdl {

int Runtime::run(int argc, char* argv[]) {
    CrashGuard::install();
#if defined(__APPLE__)
    AppleApplication::prepare();
#endif
    const int status = SDL_RunApp(argc, argv, &Runtime::enter, nullptr);
    return status != 0 ? status : Player::getExitStatus();
}

// A context of OpenGL that the GPU lost takes no more commands, so the device that replaces a lost one draws through a new context.
std::unique_ptr<gpu::Device> Runtime::createDevice() {
    if (openedDevice != nullptr) {
        return std::move(openedDevice);
    }

    if (recovering) {
        recovering = false;
        window.renewGlContext();
    }

    return gpu::DeviceFactory::create(window.getApi(), window.getSurface(), getDeviceOptions());
}

bool Runtime::prepareDevice() {
    return openedDevice != nullptr || gpu::DeviceFactory::prepare(window.getApi(), getDeviceOptions());
}

void Runtime::post(const platform::Event& event) {
    posted.post(event);
}

void Runtime::scroll(math::Vec2 value, bool precise, int modifiers) {
    if (running == nullptr || !running->started) {
        return;
    }

    running->post({.type = platform::Event::Type::MouseScroll, .modifiers = PageModifiers::read(modifiers), .scroll = value, .preciseScroll = precise});
}

void Runtime::requestQuit() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

// An app that the debugger halts never goes on once the system ends the process, so nothing of it runs as it ends.
void Runtime::terminate() {
    if (Player::isHalted()) {
        return;
    }

    Player::handleEvent({.type = platform::Event::Type::QuitRequested});
    closing = true;
    quit();
}

// SDL keeps the events that it reads during a halt in its queue for the frames after it. The events that the platform needs at once, such as the moves to the background, and every frame that UIKit or a live resize asks for come back into the runtime during the pump, where the player holds the events and the frames do nothing.
bool Runtime::pumpHaltedEvents() {
    SDL_PumpEvents();
    return closing || SDL_HasEvent(SDL_EVENT_QUIT) || SDL_HasEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED) || SDL_HasEvent(SDL_EVENT_TERMINATING);
}

int Runtime::enter(int argc, char* argv[]) {
    return SDL_EnterAppMainCallbacks(argc, argv, &Runtime::onInitialize, &Runtime::onIterate, &Runtime::onEvent, &Runtime::onQuit);
}

// SDL hands the state to the quit callback also when the initialization fails, so the runtime exists before it initializes.
SDL_AppResult Runtime::onInitialize(void** state, int argc, char* argv[]) {
    auto* runtime = new Runtime();
    *state = runtime;
    return runtime->initialize(argc, argv);
}

SDL_AppResult Runtime::onIterate(void* state) {
    if (Player::isHalted()) {
        return SDL_APP_CONTINUE;
    }

    return static_cast<Runtime*>(state)->iterate();
}

// The filter runs as UIKit hands over the press, so a press of the back button that the system takes goes on to UIKit, which leaves the app.
bool Runtime::onFilter(void* state, SDL_Event* event) {
    auto* runtime = static_cast<Runtime*>(state);

    if (!Events::isPlatformBack(*event)) {
        return true;
    }

    switch (Player::routeBack(event->type == SDL_EVENT_KEY_DOWN)) {
    case Player::Back::System:
        return false;
    case Player::Back::Player:
        ++runtime->takenBacks;
        return true;
    default:
        return true;
    }
}

SDL_AppResult Runtime::onEvent(void* state, SDL_Event* event) {
    return static_cast<Runtime*>(state)->handle(*event);
}

// The process ends right after, so an app that the debugger halts ends with it without going on.
void Runtime::onQuit(void* state, SDL_AppResult) {
    if (Player::isHalted()) {
        return;
    }

    auto* runtime = static_cast<Runtime*>(state);
    runtime->quit();
    delete runtime;
}

// Debug builds validate every call of the graphics API, and the app picks the GPU of a machine that has two.
gpu::DeviceOptions Runtime::getDeviceOptions() noexcept {
#if defined(NDEBUG)
    constexpr bool validation = false;
#else
    constexpr bool validation = true;
#endif
    return {.validation = validation, .lowPower = Player::getOpeningConfig().window.lowPowerGpu};
}

SDL_AppResult Runtime::initialize(int argc, char* argv[]) {
    running = this;
#if defined(_WIN32)
    // SDL makes the process aware of the DPI of each monitor as it starts, while the player reads the monitors before, so the process is aware from the start and the monitors have the coordinates of the window.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    try {
        auto created = std::make_unique<Host>(*this);
        host = created.get();
        Player::create(argc, argv, std::move(created));
        playerCreated = true;
    } catch (const std::exception& error) {
        return failStart(std::format("The player could not start: {}", error.what()));
    }

    if (Player::isLaunchForwarded()) {
        return SDL_APP_SUCCESS;
    }

    const core::AppConfig& config = Player::getOpeningConfig();
    window.setMaxPixelDensity(config.window.maxPixelDensity);
    transparent = config.window.transparent;
    Hints::apply(config);

    if (!SDL_Init(kSubsystems)) {
        return failStart("The window system of this machine could not start, so the app cannot open its window.");
    }

    posted.start();
#if defined(__linux__)
    waylandType = SDL_RegisterEvents(1);
#endif
#if defined(__APPLE__) && TARGET_OS_OSX
    // The main callbacks wait for events in the run loop of AppKit, and the frames come from the display link of the screen as events too, so the main thread waits between the frames as the one of every native app does.
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "waitevent");
    frameType = SDL_RegisterEvents(1);
    updateCallbackRate();
    AppleFramePacer::start(frameType);
#endif

    // AppKit and UIKit lay out the views of the services over the window as soon as they create it, before the first frame initializes the app.
    platform::NativeWindow::install(&window);
    apiFallback.emplace(Player::getGraphicsApis(), "No graphics API could start on this machine, so the app cannot open its window.");
    return start();
}

// Without a player or a window there is no error screen, so a start that fails goes to the log and to the page, which would otherwise wait for an app that never starts.
SDL_AppResult Runtime::failStart(const std::string& reason) {
    core::Log::error("{}", reason);
    Services::reportError(core::AppError(reason).toJson());
    return SDL_APP_FAILURE;
}

// The app starts once its window opened, which may take some frames while a graphics API prepares, such as WebGPU, whose adapter the browser hands over in a later turn of its event loop.
SDL_AppResult Runtime::start() {
    try {
        if (!openWindow()) {
            return SDL_APP_CONTINUE;
        }
    } catch (const std::exception& error) {
        return failStart(error.what());
    }
#if defined(__APPLE__)
    AppleApplication::finishLaunching(*this);
#endif
#if defined(__APPLE__) && TARGET_OS_OSX
    AppleFramePacer::followScreen();
#endif
#if defined(__APPLE__) && TARGET_OS_TV
    SDL_SetEventFilter(&Runtime::onFilter, this);
#endif
#if defined(__linux__)
    // A window of a Wayland session takes its frames by the frame callbacks of its surface, which wake the main callbacks as events.
    waylandPacer = WaylandFramePacer::start(window.get(), waylandType, kHiddenRate);
    updateCallbackRate();
#endif
    Player::initialize();
    started = true;

    if (!isHiddenUntilComplete()) {
        window.show();
    }

    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void Runtime::showWhenComplete() {
    if (isHiddenUntilComplete()) {
        window.show();
    }
}

// A desktop window, which no launch screen covers, stays hidden until the app drew its first complete frame, so it never shows empty. The windows of phones, televisions and pages show at once under the splash screen of their platform. A Wayland window shows at once too, since Wayland takes the frames of a window only once it shows and maps it with the first frame it takes, so it shows nothing before.
bool Runtime::isHiddenUntilComplete() noexcept {
    return Services::hasDesktop() && !WaylandDisplays::isSession();
}

// The fallback opens the window and starts the device of the first graphics API of the app that starts. The services of the platform place the window where `haylen.json` puts it on the desktop, since they know the desktop points of each monitor. A window without area, such as a canvas that its page hides, holds the start until it has one, so no device starts on a drawable without pixels.
bool Runtime::openWindow() {
#if defined(__APPLE__)
    AppleApplication::prepareWindow(Player::getOpeningConfig());
#endif
    FallbackWindow target(window);
    openedDevice = apiFallback->open(target);
    return openedDevice != nullptr;
}

bool Runtime::FallbackWindow::prepare(gpu::Device::Api api) {
    return gpu::DeviceFactory::prepare(api, getDeviceOptions());
}

gpu::Device::Api Runtime::FallbackWindow::getApi() const noexcept {
    return window.getApi();
}

void Runtime::FallbackWindow::open(gpu::Device::Api api) {
    window.open(Player::getOpeningConfig().window, api);
    if (const std::optional<math::Rect> frame = Player::getOpeningFrame()) {
        Services::setWindowFrame(*frame);
    }
}

bool Runtime::FallbackWindow::hasArea() {
    window.refresh();
    return window.getSurfaceSize().hasArea();
}

std::unique_ptr<gpu::Device> Runtime::FallbackWindow::createDevice(gpu::Device::Api api) {
    return gpu::DeviceFactory::create(api, window.getSurface(), getDeviceOptions());
}

void Runtime::FallbackWindow::close() noexcept {
    window.close();
}

// A runtime that closes never starts again, such as one that the system stopped at once from outside the frames, which AppKit does as it handles the quit of the Dock and of logging out.
SDL_AppResult Runtime::iterate() {
    if (closing) {
        return SDL_APP_SUCCESS;
    }

#if defined(__APPLE__) && TARGET_OS_OSX
    // The events of the window wake the main callbacks too, and only a frame of the display link or the timer plays.
    if (!AppleFramePacer::takeFrame()) {
        return SDL_APP_CONTINUE;
    }
#endif
#if defined(__linux__)
    // The events of a Wayland session wake the main callbacks too, and only a frame of a frame callback or of the timer of the pacer plays.
    if (waylandPacer != nullptr && !waylandPacer->takeFrame()) {
        return SDL_APP_CONTINUE;
    }
#endif

    if (!started) {
        return start();
    }

    const std::uint64_t now = SDL_GetTicksNS();
    if (!frameRate.takeFrame(static_cast<double>(now) / 1.0e9)) {
#if defined(__linux__)
        if (waylandPacer != nullptr) {
            waylandPacer->skipFrame();
        }
#endif
        return SDL_APP_CONTINUE;
    }

    recover();
#if defined(__linux__)
    // A frame of a Wayland session draws only while the compositor takes frames, so the swapchain never waits for a compositor that holds its callbacks.
    const bool presentable = waylandPacer == nullptr || waylandPacer->beginFrame();
    host->setTakingFrames(presentable);
#endif
    Player::frame(timer.next(now));
#if defined(__linux__)
    if (waylandPacer != nullptr && presentable) {
        waylandPacer->endFrame(Player::isDrawing());
    }
#endif
    followPresents();
#if defined(__APPLE__) && !TARGET_OS_OSX && !defined(NDEBUG)
    AppleApplication::checkSurface(surfaceCheck);
#endif
    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

#if defined(__APPLE__) && !TARGET_OS_OSX
void Runtime::playBackgroundFrame() {
    if (!started || closing) {
        return;
    }

    Player::frame(timer.next(SDL_GetTicksNS()));
}
#endif

// A device that stopped working, after a reset of the GPU, the removal of the adapter or a lost context, gives way to a new one, which the app starts again on once the graphics API prepared it. A device gets one replacement, and a replacement that stops working too shows a GPU that keeps no device for the app, which then ends with the reason.
void Runtime::recover() {
    const gpu::Device* device = Player::getGpuDevice();
    if (device == nullptr || !device->isLost()) {
        return;
    }

    if (const std::optional<std::string> failure = deviceRecovery.recordLoss(window.getApi())) {
        Player::fail(*failure);
        return;
    }

    recovering = true;
    Player::restart();
}

// Controllers open as SDL connects them, also the ones connected before the app started, while the other events before the app started find no app to hear them.
SDL_AppResult Runtime::handle(const SDL_Event& event) {
#if defined(__APPLE__) && TARGET_OS_OSX
    if (event.type == frameType) {
        AppleFramePacer::receiveWake();
        return SDL_APP_CONTINUE;
    }
#endif
#if defined(__linux__)
    if (event.type == waylandType) {
        if (waylandPacer != nullptr) {
            waylandPacer->receiveWake(event.user);
        }

        return SDL_APP_CONTINUE;
    }
#endif
#if !defined(__APPLE__) || TARGET_OS_OSX
    gamepads.handle(event);
#endif
    if (!started) {
        return SDL_APP_CONTINUE;
    }

    if (posted.isMarker(event)) {
        Player::handleEvent(posted.take());
        return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
    }

    // The focus of every window of the app, also of the window of an output and of a window that closed, reaches the translation, which tells the app only when the keyboard comes into the windows of the app or leaves them all.
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        if (event.window.windowID == SDL_GetWindowID(window.get())) {
            followWindow(event);
        }

        deliver(event);
        return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
    }

    if (outputs.handle(event)) {
        return SDL_APP_CONTINUE;
    }

    // The events that SDL still holds for a window that is gone belong to no window of the app.
    if (Events::isOfClosedWindow(event)) {
        return SDL_APP_CONTINUE;
    }

    followWindow(event);

    if (takes(event)) {
        deliver(event);
    }

    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// The back button that the player took reaches no app. A request to quit closes the window once the app heard it, as an app that quits itself does.
void Runtime::deliver(const SDL_Event& event) {
    if (Events::isPlatformBack(event) && takenBacks > 0) {
        --takenBacks;
        return;
    }

    translated.clear();
    events.translate(event, getContext(), translated);

    for (const platform::Event& translatedEvent : translated) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
        // Screen readers follow the focus of the window that is active, which the window tells them as it gains and loses the focus.
        if (translatedEvent.type == platform::Event::Type::FocusGained || translatedEvent.type == platform::Event::Type::FocusLost) {
            LinuxAccessibility::setActive(translatedEvent.type == platform::Event::Type::FocusGained);
        }
#endif
        Player::handleEvent(translatedEvent);

        if (translatedEvent.type == platform::Event::Type::QuitRequested) {
            closing = true;
        }
    }
}

// A runtime that ends before its app started, such as one whose window system or graphics API could not start or that handed its link to the instance that runs, cleans up the player it made too.
void Runtime::quit() noexcept {
    started = false;
    if (std::exchange(playerCreated, false)) {
        Player::cleanup();
    }

#if defined(__APPLE__) && TARGET_OS_OSX
    AppleFramePacer::stop();
#endif
#if defined(__linux__)
    waylandPacer.reset();
#endif
    host = nullptr;
    platform::NativeWindow::install(nullptr);
    openedDevice.reset();
    outputs.closeAll();
    window.close();
    running = nullptr;
}

// The runtime of macOS reads the scrolls of trackpads with their phases on its own, and the keys that reach the native field that edits a text field of the UI belong to that field, except the release of a key that the app heard go down before the field took the keyboard. UIKit apps take their typed text from the native field alone, and the runtime reads the scrolls of a pointer that the scene does not lock from UIKit, so the wheel of SDL reports only the scrolls of a locked pointer and of the mice of tvOS there. Fingers count only on the screen itself, not on the trackpads that SDL also reports.
bool Runtime::takes(const SDL_Event& event) const noexcept {
    switch (event.type) {
#if defined(__APPLE__) && TARGET_OS_OSX
    case SDL_EVENT_MOUSE_WHEEL:
        return false;
    case SDL_EVENT_KEY_UP:
        return !AppleApplication::isTextFieldFocused() || events.isHeld(event.key.scancode);
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
        return !AppleApplication::isTextFieldFocused();
#elif defined(__APPLE__)
    case SDL_EVENT_TEXT_INPUT:
        return false;
#elif defined(__EMSCRIPTEN__)
    case SDL_EVENT_MOUSE_WHEEL:
        return false;
#endif
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return event.tfinger.touchID == SDL_PEN_TOUCHID || SDL_GetTouchDeviceType(event.tfinger.touchID) == SDL_TOUCH_DEVICE_DIRECT;
    default:
        return true;
    }
}

Events::Context Runtime::getContext() const noexcept {
    const bool appHoldsKeyboard = SDL_GetKeyboardFocus() == window.get() || outputs.holdsFocus();
    return {.density = window.getPixelDensity(), .framebuffer = window.getPixelSize(), .modifiers = SDL_GetModState(), .appHoldsKeyboard = appHoldsKeyboard};
}

// The swapchain follows the drawable as its size or scale changes, the frames slow down while the window shows nothing, since no display paces them then, and the host follows the window into fullscreen and out of it. The Metal view of UIKit reports the size of its drawable on its own, such as when the scene turns to the orientation of the app after the window opened, before SDL reports the window resized.
void Runtime::followWindow(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED: {
        followDrawable();
        return;
    }
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_OCCLUDED:
    case SDL_EVENT_WINDOW_EXPOSED: {
        const bool nowHidden = (SDL_GetWindowFlags(window.get()) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED)) != 0U;
        if (nowHidden != hidden) {
            hidden = nowHidden;
            updateCallbackRate();
        }

#if defined(__linux__)
        // SDL reports a window that the compositor suspends as occluded, and it presents nothing until it shows again.
        if (waylandPacer != nullptr) {
            waylandPacer->setSuspended(nowHidden);
        }
#endif

        // A window that shows again may have lost its picture, such as one of X11 without a compositor, so an app that redraws on change draws it again.
        if (event.type == SDL_EVENT_WINDOW_EXPOSED) {
            Player::requestRedraw();
        }

        return;
    }
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        host->followFullscreen(event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
        return;
#if defined(__APPLE__) && TARGET_OS_OSX
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        AppleFramePacer::followScreen();
        return;
#endif
#if defined(_WIN32) || defined(__linux__)
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        textInput.restore();
        return;
#endif
    default:
        return;
    }
}

void Runtime::setMaxPixelDensity(float cap) {
    window.setMaxPixelDensity(cap);
    followDrawable();
}

// A window without area, such as a canvas that its page hides, leaves the device with the last surface that had one, since the engine draws nothing until the window has an area again.
void Runtime::followDrawable() {
    window.refresh();
    const gpu::Surface::Size size = window.getSurfaceSize();
    gpu::Device* device = Player::getGpuDevice();
    if (device != nullptr && size.hasArea()) {
        device->resizeSurface(size);
    }
}

void Runtime::setFrameRate(int framesPerSecond) {
    frameRate.setTarget(framesPerSecond);
    updateCallbackRate();
}

// The iterations follow the frame rate of the app, which SDL keeps by sleeping between them on Windows and Linux and by the rate of the display link on UIKit, and the present of each frame paces them without one. On macOS the display link of the screen of the window plays the frames at that rate. A hidden window, which no display paces, runs them at most at `kHiddenRate`, and on Windows and Linux the frames that present nothing run at the refresh of the display of the window, so input still reaches the app at once. The page keeps the animation frames of the browser, which the frame rate thins out, since the timers that would replace them fire late.
void Runtime::updateCallbackRate() {
    const int target = frameRate.getTarget();
    const int hiddenRate = target > 0 ? std::min(target, kHiddenRate) : kHiddenRate;
#if defined(__APPLE__) && TARGET_OS_OSX
    AppleFramePacer::pace(target, hiddenRate, hidden);
#else
#if defined(__linux__)
    // The frame callbacks and the timer of the pacer wake the main callbacks of a Wayland session, which wait for events between them.
    if (waylandPacer != nullptr) {
        waylandPacer->setHiddenRate(hiddenRate);
        SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "waitevent");
        return;
    }
#endif
    int rate = hidden ? hiddenRate : target;
#if defined(__EMSCRIPTEN__)
    if (!hidden) {
        rate = 0;
    }
#elif !defined(__APPLE__)
    if (!hidden && !presenting && target == 0) {
        rate = getRefreshRate();
    }
#endif
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, std::to_string(rate).c_str());
#endif
}

// A frame that draws nothing presents nothing that the display could pace, so the rate of the iterations changes whenever the app starts or stops presenting.
void Runtime::followPresents() {
    const bool presented = Player::isDrawing();
    if (presented == presenting) {
        return;
    }

    presenting = presented;
    updateCallbackRate();
}

// A display that reports no refresh paces like a hidden window.
int Runtime::getRefreshRate() const {
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window.get()));
    if (mode == nullptr || mode->refresh_rate <= 0.0F) {
        return kHiddenRate;
    }

    return static_cast<int>(std::lround(mode->refresh_rate));
}

} // namespace haylen::platform::sdl

#if defined(__APPLE__)
// Apple apps call this from their main function after registering their native HaylenBridge handlers.
extern "C" int haylen_main(int argc, char* argv[]) {
    haylen::platform::Services::install(haylen::platform::AppleServices::create());
    return haylen::platform::sdl::Runtime::run(argc, argv);
}
#elif defined(_WIN32)
// Windows apps run in the windows subsystem, and SDL reads their command line as UTF-8.
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    if (const char* missing = haylen::platform::ProcessorCheck::describeMissing(haylen::platform::ProcessorCheck::readFeatures())) {
        MessageBoxA(nullptr, missing, nullptr, MB_OK | MB_ICONERROR);
        return 1;
    }

    haylen::platform::Services::install(haylen::platform::WindowsServices::create());
    return haylen::platform::sdl::Runtime::run(__argc, __argv);
}
#elif !defined(__EMSCRIPTEN__)
int main(int argc, char* argv[]) {
    if (!haylen::platform::ProcessorCheck::require()) {
        return 1;
    }

    haylen::platform::Services::install(haylen::platform::LinuxServices::create());
    return haylen::platform::sdl::Runtime::run(argc, argv);
}
#endif

#if defined(__EMSCRIPTEN__)
extern "C" {

// The page reads the scrolls of the wheels and trackpads over the canvas with their units, which SDL leaves out. The modifiers are the bits of shift, control, alt and the super key, from the lowest.
EMSCRIPTEN_KEEPALIVE void haylen_web_scroll(float x, float y, int precise, int modifiers) {
    haylen::platform::WebEntry::guard("haylen_web_scroll", [&] { haylen::platform::sdl::Runtime::scroll({x, y}, precise != 0, modifiers); });
}
}
#endif
