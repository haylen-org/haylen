#include "backends/sokol/Runtime.hpp"

#include <cmath>
#include <memory>
#include <optional>

#include "backends/sokol/Events.hpp"
#include "backends/sokol/Host.hpp"
#include "platform/Player.hpp"
#include "sokol_log.h"

#if defined(__APPLE__)
#include "backends/sokol/apple/AppleClasses.hpp"
#include "haylen/platform/apple/HaylenMain.h"
#elif defined(__ANDROID__)
#include <game-activity/GameActivity.h>

#include "platform/android/AndroidGlesVersion.hpp"
#include "platform/android/AndroidInputEvents.hpp"
#elif defined(__EMSCRIPTEN__)
#include <string>

#include "platform/web/WebPage.hpp"
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdlib>
#endif

namespace haylen::sokol {

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
int Runtime::run(int argc, char* argv[]) {
    sapp_desc desc = describe(argc, argv);
#if defined(__APPLE__)
    desc.apple.delegate_class = AppleClasses::getDelegateClass();
    desc.apple.view_controller_class = AppleClasses::getViewControllerClass();
#endif
    sapp_run(&desc);
    return platform::Player::getExitStatus();
}
#endif

sapp_desc Runtime::describe(int argc, char* argv[]) {
#if defined(_WIN32) && defined(SOKOL_D3D11)
    // The library `sokol_app` makes D3D11 apps aware of the DPI of each monitor when their window opens, so the process is aware from the start and the monitors the player reads before the window exists have the coordinates of the window.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    platform::Player::create(argc, argv, std::make_unique<Host>());
    const core::AppConfig& config = platform::Player::getOpeningConfig();

    // AppKit and UIKit lay out the views of the services over the window as soon as they create it, before the first frame initializes the app.
    platform::NativeWindow::install(&getWindow());
    sapp_desc desc{};
    desc.init_cb = &onInitialize;
    desc.frame_cb = &onFrame;
    desc.event_cb = &onEvent;
    desc.cleanup_cb = &onCleanup;
    desc.width = config.window.width;
    desc.height = config.window.height;
    desc.sample_count = config.window.sampleCount;
    desc.swap_interval = config.window.vsync ? 1 : 0;
    desc.metal.disable_display_sync = !config.window.vsync;
    desc.high_dpi = config.window.highDpi;
    desc.fullscreen = config.window.fullscreen;
    desc.window_title = config.window.title.c_str();
    // Linux desktops find the desktop entry of the app by the class of its window, which the Linux template names after the identifier.
    desc.desktop.app_id = config.identifier.c_str();
    desc.enable_clipboard = true;
    desc.clipboard_size = 64 * 1024;
    desc.logger.func = slog_func;

    // The window opens with its desktop options already in effect, so a frameless window never shows a title bar for its first frame. Whether it can be transparent lasts for the whole run.
    desc.composite_mode = config.window.transparent ? SAPP_COMPOSITEMODE_PREMULTIPLIED : SAPP_COMPOSITEMODE_OPAQUE;
    desc.desktop.borderless = !config.window.decorated;
    desc.desktop.topmost = config.window.alwaysOnTop;
    desc.desktop.no_focus = !config.window.focusable;
    desc.desktop.skip_taskbar = !config.window.showInTaskbar;
    if (const std::optional<math::Rect> frame = platform::Player::getOpeningFrame()) {
        desc.width = static_cast<int>(std::lround(frame->width));
        desc.height = static_cast<int>(std::lround(frame->height));
        desc.desktop.has_position = true;
        desc.desktop.x = static_cast<int>(std::lround(frame->x));
        desc.desktop.y = static_cast<int>(std::lround(frame->y));
    }
#if defined(__ANDROID__)
    const platform::AndroidGlesVersion gles = platform::AndroidGlesVersion::read();
    desc.gl.major_version = gles.major;
    desc.gl.minor_version = gles.minor;
    desc.android.native_event_cb = &onNativeEvent;
    desc.android.key_consumed_cb = &onKeyConsumed;
#elif defined(__EMSCRIPTEN__)
    // The selector outlives the call, since `sokol_app` reads it when the page starts the app.
    static const std::string& canvas = *new std::string(platform::WebPage::getCanvasSelector());
    desc.html5.canvas_selector = canvas.c_str();
    desc.html5.update_document_title = true;
#endif
    return desc;
}

Window& Runtime::getWindow() noexcept {
    static Window& window = *new Window();
    return window;
}

void Runtime::onInitialize() {
    platform::Player::initialize();
}

void Runtime::onFrame() {
    platform::Player::frame(sapp_frame_duration());
}

// The system leaves the app on a press of the back button that neither the app nor the player takes.
void Runtime::onEvent(const sapp_event* source) {
    if (Events::isPlatformBack(*source)) {
        const platform::Player::Back back = platform::Player::routeBack(source->type == SAPP_EVENTTYPE_KEY_DOWN);
        if (back == platform::Player::Back::System) {
            return;
        }
        sapp_consume_event();
        if (back == platform::Player::Back::Player) {
            return;
        }
    }
    if (const std::optional<platform::Event> translated = Events::translate(*source)) {
        platform::Player::handleEvent(*translated);
    }
}

void Runtime::onCleanup() {
    platform::Player::cleanup();
    platform::NativeWindow::install(nullptr);
}

#if defined(__ANDROID__)
bool Runtime::onNativeEvent(const void* source) {
    const auto& event = *static_cast<const sapp_android_native_event*>(source);
    if (event.type == SAPP_ANDROID_NATIVE_EVENT_KEY) {
        return platform::AndroidInputEvents::takeKey(*static_cast<const GameActivityKeyEvent*>(event.event));
    }
    return platform::AndroidInputEvents::takeMotion(*static_cast<const GameActivityMotionEvent*>(event.event));
}

bool Runtime::onKeyConsumed(const void* source) {
    return platform::AndroidInputEvents::takesKey(*static_cast<const GameActivityKeyEvent*>(source));
}
#endif

} // namespace haylen::sokol

#if defined(__APPLE__)
// The library sokol_app leaves main to Apple apps, which call this after registering their native HaylenBridge handlers.
int haylen_main(int argc, char* argv[]) {
    return haylen::sokol::Runtime::run(argc, argv);
}
#elif defined(__ANDROID__) || defined(__EMSCRIPTEN__)
sapp_desc sokol_main(int argc, char* argv[]) {
    return haylen::sokol::Runtime::describe(argc, argv);
}
#elif defined(_WIN32)
// Windows apps run in the windows subsystem, and the UTF-8 code page of the manifest of the engine gives them their command line in UTF-8.
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    return haylen::sokol::Runtime::run(__argc, __argv);
}
#else
int main(int argc, char* argv[]) {
    return haylen::sokol::Runtime::run(argc, argv);
}
#endif
