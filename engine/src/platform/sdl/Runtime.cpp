#include "platform/sdl/Runtime.hpp"

#include <exception>
#include <stdexcept>

// The runtime holds the entry points itself and enters the main callbacks of SDL through the functions they name.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_touch.h>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"
#include "platform/sdl/Host.hpp"

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
#if defined(__APPLE__)
    AppleApplication::prepare();
#endif
    const int status = SDL_RunApp(argc, argv, &Runtime::enter, nullptr);
    return status != 0 ? status : Player::getExitStatus();
}

std::unique_ptr<gpu::Device> Runtime::createDevice() {
    if (openedDevice != nullptr) {
        return std::move(openedDevice);
    }
    return gpu::DeviceFactory::create(window.getApi(), window.getSurface(), getDeviceOptions());
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

void Runtime::terminate() {
    Player::handleEvent({.type = platform::Event::Type::QuitRequested});
    quit();
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

void Runtime::onQuit(void* state, SDL_AppResult) {
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

gpu::DeviceFactory::Options Runtime::getDeviceOptions() noexcept {
#if defined(NDEBUG)
    return {.validation = false};
#else
    return {.validation = true};
#endif
}

SDL_AppResult Runtime::initialize(int argc, char* argv[]) {
    running = this;
#if defined(_WIN32)
    // SDL makes the process aware of the DPI of each monitor as it starts, while the player reads the monitors before, so the process is aware from the start and the monitors have the coordinates of the window.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    Player::create(argc, argv, std::make_unique<Host>(*this));
    const core::AppConfig& config = Player::getOpeningConfig();
    highDpi = config.window.highDpi;
    transparent = config.window.transparent;
    describeApp(config);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    // Desktops and the web type text into the window itself, while UIKit apps type into the native fields of the services alone.
#if !defined(__APPLE__) || TARGET_OS_OSX
    SDL_StartTextInput(window.get());
#endif

    Player::initialize();
    started = true;
    window.show();
    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// Each graphics API in the order of the app opens a window with what it draws through and starts its device, and the first that starts keeps the window. The reasons of the ones that failed reach the log, so a machine that falls back says why. The services of the platform place the window where `app.json` puts it on the desktop, since they know the desktop points of each monitor.
bool Runtime::openWindow() {
    const core::AppConfig& config = Player::getOpeningConfig();
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
    if (!closing) {
        Player::frame(timer.next(SDL_GetTicksNS()));
    }
    return closing ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// The events before the app started find no app to hear them.
SDL_AppResult Runtime::handle(const SDL_Event& event) {
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
    platform::NativeWindow::install(nullptr);
    openedDevice.reset();
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

// The swapchain follows the drawable as its size or scale changes, and the frames slow down while the window shows nothing, since no display paces them then.
void Runtime::followWindow(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: {
        window.refresh();
        const math::Vec2 size = window.getPixelSize();
        if (gpu::Device* device = Player::getGpuDevice()) {
            device->resizeSurface(static_cast<int>(size.x), static_cast<int>(size.y));
        }
        return;
    }
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_OCCLUDED:
    case SDL_EVENT_WINDOW_EXPOSED: {
        const bool hidden = (SDL_GetWindowFlags(window.get()) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED)) != 0U;
        if (hidden != paced) {
            paced = hidden;
            SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, hidden ? kHiddenRate : "0");
        }
        return;
    }
    default:
        return;
    }
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
