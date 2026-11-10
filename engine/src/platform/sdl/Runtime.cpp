#include "platform/sdl/Runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
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

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/Error.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"
#include "platform/sdl/Host.hpp"
#include "platform/sdl/WaylandDisplays.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>

#include "haylen/platform/apple/HaylenMain.h"
#include "platform/sdl/apple/AppleApplication.hpp"
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

#include "platform/web/WebEntry.hpp"
#include "platform/web/WebPage.hpp"
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>
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
    posted.push_back(event);
    SDL_Event marker{};
    marker.type = postedType;
    SDL_PushEvent(&marker);
}

void Runtime::scroll(math::Vec2 value, bool precise, int modifiers) {
    if (running == nullptr || !running->started) {
        return;
    }
    const input::KeyModifiers held{.shift = (modifiers & 1) != 0, .control = (modifiers & 2) != 0, .alt = (modifiers & 4) != 0, .super = (modifiers & 8) != 0};
    running->post({.type = platform::Event::Type::MouseScroll, .modifiers = held, .scroll = value, .preciseScroll = precise});
}

// An app that the debugger halts never goes on once the system ends the process, so nothing of it runs as it ends.
void Runtime::terminate() {
    if (Player::isHalted()) {
        return;
    }
    Player::handleEvent({.type = platform::Event::Type::QuitRequested});
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

// SDL names the app after its identifier, which Linux desktops match with the desktop entry of the app through the class of its window. The screen saver keeps its system settings, touches stay touches and the first click on a window in the background reaches the app, as with every native app of the platform.
void Runtime::describeApp(const core::AppConfig& config) {
    SDL_SetAppMetadata(config.window.title.c_str(), nullptr, config.identifier.c_str());
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#if !defined(__APPLE__) || TARGET_OS_OSX
    // Controllers reach the app also while its window has no focus, such as a window that never takes it above the taskbar.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif
#if defined(__APPLE__) && TARGET_OS_TV
    // The presses of the Apple TV remote reach UIKit, which leaves the app on the Menu press that the app does not take.
    SDL_SetHint(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS, "1");
#elif defined(__EMSCRIPTEN__)
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, WebPage::getCanvasSelector().c_str());
#elif defined(__linux__)
    // Linux apps open their window on X11 first, also through XWayland in a Wayland session, since Wayland lets no app place its window, keep it above the others, leave the taskbar or let clicks through. A session without X11 opens it on Wayland, and the variable `SDL_VIDEO_DRIVER` picks either one.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11,wayland");
#endif
}

// Debug builds validate every call of the graphics API, and the app picks the GPU of a machine that has two.
gpu::DeviceFactory::Options Runtime::getDeviceOptions() noexcept {
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
    // Without a player there is no window and no error screen, so a failure to create one goes to the log and the page, which would otherwise wait for an app that never starts.
    try {
        auto created = std::make_unique<Host>(*this);
        host = created.get();
        Player::create(argc, argv, std::move(created));
    } catch (const std::exception& error) {
        core::Log::error("The player could not start: {}", error.what());
        Services::reportError(lua::Error(error.what()).toJson());
        return SDL_APP_FAILURE;
    }
    if (Player::isLaunchForwarded()) {
        return SDL_APP_SUCCESS;
    }
    const core::AppConfig& config = Player::getOpeningConfig();
    window.setMaxPixelDensity(config.window.maxPixelDensity);
    transparent = config.window.transparent;
    describeApp(config);
    if (!SDL_Init(kSubsystems)) {
        core::Log::error("The window system could not start. {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    postedType = SDL_RegisterEvents(1);

    // AppKit and UIKit lay out the views of the services over the window as soon as they create it, before the first frame initializes the app.
    platform::NativeWindow::install(&window);
    apis = Player::getGraphicsApis();
    return start();
}

// The app starts once its window opened, which may take some frames while a graphics API prepares, such as WebGPU, whose adapter the browser hands over in a later turn of its event loop.
SDL_AppResult Runtime::start() {
    try {
        if (!openWindow()) {
            return SDL_APP_CONTINUE;
        }
    } catch (const std::exception& error) {
        core::Log::error("{}", error.what());
        return SDL_APP_FAILURE;
    }
#if defined(__APPLE__)
    AppleApplication::finishLaunching(*this);
#endif
#if defined(__APPLE__) && TARGET_OS_TV
    SDL_SetEventFilter(&Runtime::onFilter, this);
#endif
    Player::initialize();
    started = true;
    if (!isHiddenUntilComplete()) {
        window.show();
    }
    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void Runtime::endSplashScreen(float fadeOutSeconds) {
    if (isHiddenUntilComplete()) {
        window.show();
    }
    Services::endSplashScreen(fadeOutSeconds);
}

// A desktop window, which no launch screen covers, stays hidden until the app drew its first complete frame, so it never shows empty. The windows of phones, televisions and pages show at once under the splash screen of their platform. A Wayland window shows at once too, since Wayland takes the frames of a window only once it shows and maps it with the first frame it takes, so it shows nothing before.
bool Runtime::isHiddenUntilComplete() noexcept {
    return Services::hasDesktop() && !WaylandDisplays::isSession();
}

// Each graphics API in the order of the app opens a window with what it draws through and starts its device, and the first that starts keeps the window. The reasons of the ones that failed reach the log, so a machine that falls back says why. The services of the platform place the window where `app.json` puts it on the desktop, since they know the desktop points of each monitor.
bool Runtime::openWindow() {
    const core::AppConfig& config = Player::getOpeningConfig();
#if defined(__APPLE__)
    AppleApplication::prepareWindow(config);
#endif
    for (; nextApi < apis.size(); ++nextApi) {
        const gpu::Device::Api api = apis[nextApi];
        if (!gpu::DeviceFactory::prepare(api, getDeviceOptions())) {
            return false;
        }
        try {
            window.open(config.window, api);
            if (const std::optional<math::Rect> frame = Player::getOpeningFrame()) {
                Services::setWindowFrame(*frame);
                window.refresh();
            }
            openedDevice = gpu::DeviceFactory::create(api, window.getSurface(), getDeviceOptions());
            core::Log::info("The app draws through \"{}\".", gpu::DeviceFactory::getApiName(api));
            return true;
        } catch (const std::exception& error) {
            core::Log::warning("The graphics API \"{}\" could not start. {}", gpu::DeviceFactory::getApiName(api), error.what());
            window.close();
        }
    }
    throw std::runtime_error("No graphics API could start on this machine, so the app cannot open its window.");
}

SDL_AppResult Runtime::iterate() {
    if (!started) {
        return start();
    }
    const std::uint64_t now = SDL_GetTicksNS();
    if (!closing && !frameRate.takeFrame(static_cast<double>(now) / 1.0e9)) {
        return SDL_APP_CONTINUE;
    }
    if (!closing) {
        recover();
        Player::frame(timer.next(now));
    }
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
#if !defined(__APPLE__) || TARGET_OS_OSX
    gamepads.handle(event);
#endif
    if (!started) {
        return SDL_APP_CONTINUE;
    }
    if (event.type == postedType) {
        Player::handleEvent(posted[postedRead++]);
        if (postedRead == posted.size()) {
            posted.clear();
            postedRead = 0;
        }
        return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
    }

    if (outputs.handle(event)) {
        return SDL_APP_CONTINUE;
    }
    // The window of the app keeps the focus while the keyboard goes to the window of an output and back.
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && outputs.holdsFocus()) {
        focusInOutput = true;
        return SDL_APP_CONTINUE;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && std::exchange(focusInOutput, false)) {
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
        Player::handleEvent(translatedEvent);
        if (translatedEvent.type == platform::Event::Type::QuitRequested) {
            closing = true;
        }
    }
}

void Runtime::quit() noexcept {
    if (started) {
        started = false;
        Player::cleanup();
    }
    host = nullptr;
    platform::NativeWindow::install(nullptr);
    openedDevice.reset();
    outputs.closeAll();
    window.close();
    running = nullptr;
}

// The runtime of macOS reads the scrolls of trackpads with their phases on its own, and the keys that reach the native field that edits a text field of the UI belong to that field. UIKit apps read hardware keyboards through GameController, so only the presses of remotes and game controllers, which SDL reports for no keyboard of their own, the keyboard 0, come from SDL there. Fingers count only on the screen itself, not on the trackpads that SDL also reports.
bool Runtime::takes(const SDL_Event& event) const noexcept {
    switch (event.type) {
#if defined(__APPLE__) && TARGET_OS_OSX
    case SDL_EVENT_MOUSE_WHEEL:
        return false;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
        return !AppleApplication::isTextFieldFocused();
#elif defined(__APPLE__)
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return event.key.which == 0;
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
    return {.density = window.getPixelDensity(), .framebuffer = window.getPixelSize(), .modifiers = SDL_GetModState()};
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
        return;
    }
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        host->followFullscreen(event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
        return;
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

void Runtime::followDrawable() {
    window.refresh();
    if (gpu::Device* device = Player::getGpuDevice()) {
        device->resizeSurface(window.getSurfaceSize());
    }
}

void Runtime::setFrameRate(int framesPerSecond) {
    frameRate.setTarget(framesPerSecond);
    updateCallbackRate();
}

// The iterations follow the frame rate of the app, which SDL keeps by sleeping between them on desktops and by the rate of the display link on UIKit, and the display paces them without one. A hidden window, which no display paces, runs them at most at `kHiddenRate`. The page keeps the animation frames of the browser, which the frame rate thins out, since the timers that would replace them fire late.
void Runtime::updateCallbackRate() {
    const int target = frameRate.getTarget();
    int rate = target;
    if (hidden) {
        rate = target > 0 ? std::min(target, kHiddenRate) : kHiddenRate;
    }
#if defined(__EMSCRIPTEN__)
    if (!hidden) {
        rate = 0;
    }
#endif
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, std::to_string(rate).c_str());
}

} // namespace haylen::platform::sdl

#if defined(__APPLE__)
// Apple apps call this from their main function after registering their native HaylenBridge handlers.
int haylen_main(int argc, char* argv[]) {
    return haylen::platform::sdl::Runtime::run(argc, argv);
}
#elif defined(_WIN32)
// Windows apps run in the windows subsystem, and SDL reads their command line as UTF-8.
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    return haylen::platform::sdl::Runtime::run(__argc, __argv);
}
#else
int main(int argc, char* argv[]) {
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
