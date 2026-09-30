#include "platform/sokol/SokolRuntime.hpp"

#include <cmath>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "platform/Services.hpp"
#include "platform/native/NativeApi.hpp"
#include "platform/sokol/MemoryWarning.hpp"
#include "platform/sokol/SokolEvents.hpp"
#include "sokol_log.h"

#if defined(__APPLE__)
#include "haylen/platform/apple/HaylenMain.h"
#include "platform/apple/AppleRuntime.hpp"
#elif defined(__ANDROID__)
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidKeys.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/JavaBridge.hpp"
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebPage.hpp"
#endif

namespace haylen::platform {

#if defined(__APPLE__)
int SokolRuntime::run(int argc, char* argv[]) {
    sapp_desc desc = describe(argc, argv);
    desc.apple.delegate_class = AppleRuntime::getDelegateClass();
    sapp_run(&desc);
    return 0;
}
#endif

sapp_desc SokolRuntime::describe(int argc, char* argv[]) {
    Services::initialize();
    Process& process = getProcess();
    process.current = std::make_unique<SokolRuntime>();
    SokolRuntime& runtime = *process.current;
    const LaunchOptions options = parseLaunchOptions(argc, argv);
    runtime.development = options.development;
    for (const std::string& folder : options.nativeFolders) {
        NativeLibraries::addSearchFolder(folder);
    }
    // clang-format off
    runtime.pending = load([&options] {
        return options.package.empty() ? Services::openBundledPackage() : std::shared_ptr<io::Package>(io::Package::open(options.package));
    }, runtime.development);
    // clang-format on
    runtime.package = runtime.pending.package;

    const core::AppConfig& config = runtime.pending.config;
    sapp_desc desc{};
    desc.user_data = &runtime;
    desc.init_userdata_cb = &onInitialize;
    desc.frame_userdata_cb = &onFrame;
    desc.event_userdata_cb = &onEvent;
    desc.cleanup_userdata_cb = &onCleanup;
    desc.width = config.window.width;
    desc.height = config.window.height;
    desc.sample_count = config.window.sampleCount;
    desc.swap_interval = config.window.vsync ? 1 : 0;
    desc.metal.disable_display_sync = !config.window.vsync;
    desc.high_dpi = config.window.highDpi;
    desc.fullscreen = config.window.fullscreen;
    desc.window_title = config.window.title.c_str();
    desc.enable_clipboard = true;
    desc.clipboard_size = 64 * 1024;
    desc.logger.func = slog_func;
    runtime.describeDesktop(desc, config.window);
#if defined(SOKOL_GLES3)
    // The shaders of the engine are GLSL ES 3.00, so OpenGL ES 3.0 is enough, while sokol_app asks Android for 3.1, which emulators and some devices lack.
    desc.gl.major_version = 3;
    desc.gl.minor_version = 0;
#endif
#if defined(__EMSCRIPTEN__)
    runtime.canvas = WebPage::getCanvasSelector();
    desc.html5.canvas_selector = runtime.canvas.c_str();
    desc.html5.update_document_title = true;
#endif
#if defined(__ANDROID__)
    desc.android.native_event_cb = &onAndroidInput;
#endif
    return desc;
}

// The window opens with its desktop options already in effect, so a frameless window never shows a title bar for its first frame. Whether it can be transparent lasts for the whole run.
void SokolRuntime::describeDesktop(sapp_desc& desc, const core::AppConfig::Window& window) {
    desc.composite_mode = window.transparent ? SAPP_COMPOSITEMODE_PREMULTIPLIED : SAPP_COMPOSITEMODE_OPAQUE;
    desc.desktop.borderless = !window.decorated;
    desc.desktop.topmost = window.alwaysOnTop;
    desc.desktop.no_focus = !window.focusable;
    desc.desktop.skip_taskbar = !window.showInTaskbar;
    host.prepare({.transparent = window.transparent, .resizable = window.resizable, .decorated = window.decorated, .alwaysOnTop = window.alwaysOnTop, .shownInTaskbar = window.showInTaskbar}, window.focusable);
    if (!window.position || !Services::hasDesktop()) {
        return;
    }

    const std::vector<Monitor> monitors = Services::getMonitors();
    const math::Rect frame = window.position->resolve(monitors, {static_cast<float>(window.width), static_cast<float>(window.height)});
    desc.width = static_cast<int>(std::lround(frame.width));
    desc.height = static_cast<int>(std::lround(frame.height));
    desc.desktop.has_position = true;
    desc.desktop.x = static_cast<int>(std::lround(frame.x));
    desc.desktop.y = static_cast<int>(std::lround(frame.y));
}

void SokolRuntime::restart(std::shared_ptr<io::Package> source) {
    restart([&source] { return std::move(source); });
}

void SokolRuntime::restart(const std::function<std::shared_ptr<io::Package>()>& open) {
    SokolRuntime& runtime = getCurrent();
    runtime.replace(load(open, runtime.development));
}

void SokolRuntime::restart() {
    restart(getCurrent().package);
}

void SokolRuntime::stop() {
    getCurrent().replace({.package = std::make_shared<io::MemoryPackage>("stopped"), .config = {}, .application = std::make_unique<StoppedApplication>(), .playing = false});
}

void SokolRuntime::setPaused(bool value) {
    SokolRuntime& runtime = getCurrent();
    if (runtime.engine == nullptr || runtime.paused == value) {
        return;
    }
    runtime.paused = value;
    runtime.engine->handleEvent({.type = value ? Event::Type::Suspended : Event::Type::Resumed});
}

bool SokolRuntime::isPaused() noexcept {
    return getCurrent().paused;
}

bool SokolRuntime::reloadAsset(std::string_view path) {
    const SokolRuntime& runtime = getCurrent();
    return runtime.engine != nullptr && runtime.engine->getAssets().reload(path) > 0;
}

void SokolRuntime::handleEvent(const Event& event) {
    getCurrent().deliver(event);
}

void SokolRuntime::postEvent(Event event) {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    process.posted.push_back(std::move(event));
}

std::vector<Event> SokolRuntime::takePostedEvents() {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    return std::exchange(process.posted, {});
}

SokolRuntime::Process& SokolRuntime::getProcess() noexcept {
    static Process& process = *new Process();
    return process;
}

SokolRuntime& SokolRuntime::getCurrent() noexcept {
    return *getProcess().current;
}

// Options other than --dev and --native come from the system, such as the ones Xcode passes to the macOS apps it launches, and are left to it.
SokolRuntime::LaunchOptions SokolRuntime::parseLaunchOptions(int argc, char* argv[]) {
    LaunchOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--dev") {
            options.development = true;
        } else if (argument == "--native" && index + 1 < argc) {
            options.nativeFolders.emplace_back(argv[++index]);
        } else if (!argument.starts_with('-') && options.package.empty()) {
            options.package = argument;
        }
    }
    return options;
}

// Reads the package configuration and creates the application, turning any failure into an app that shows the error. A package that opened stays with the failed app, so a restart tries it again. Apps in development reload when their files change.
SokolRuntime::App SokolRuntime::load(const std::function<std::shared_ptr<io::Package>()>& open, bool hotReload) {
    App app;
    try {
        app.package = open();
        app.config = core::AppConfig::fromPackage(*app.package);
        app.config.hotReload = hotReload;
        app.application = core::Application::create();
        app.application->configure(app.config);
    } catch (const std::exception& error) {
        core::Log::error("The app could not be loaded: {}", error.what());
        if (app.package == nullptr) {
            app.package = std::make_shared<io::MemoryPackage>("missing");
        }
        app.config = {};
        app.application = std::make_unique<FailedApplication>(std::string("The app could not be loaded. ") + error.what());
    }
    return app;
}

// Native libraries open windows of their own over the window of the app, which exists from here on.
void SokolRuntime::onInitialize(void* data) {
    Services::watchWindow();
    NativeApi::setWindow(Services::getNativeWindow());
    static_cast<SokolRuntime*>(data)->launch();
}

void SokolRuntime::onFrame(void* data) {
    SokolRuntime& runtime = *static_cast<SokolRuntime*>(data);
    if (runtime.paused || runtime.engine == nullptr) {
        return;
    }
    Services::updateWindow();
    if (MemoryWarning::take()) {
        runtime.engine->handleEvent({.type = Event::Type::LowMemory});
    }
    for (const Event& event : takePostedEvents()) {
        runtime.deliver(event);
    }
    runtime.engine->frame(sapp_frame_duration());
#if defined(__ANDROID__)
    AndroidActivity::endSplashScreen();
    runtime.reportBackCapture();
#endif
    if (!runtime.engine->isRunning()) {
        sapp_quit();
        return;
    }
#if defined(__EMSCRIPTEN__)
    WebPage::reportFrame(*runtime.engine);
#endif

    // A restart happens between frames, where no engine code is on the stack.
    if (runtime.engine->isRestartRequested()) {
        restart();
    }
}

bool SokolRuntime::isBackCaptured() {
    const std::unique_ptr<SokolRuntime>& current = getProcess().current;
    return current != nullptr && current->engine != nullptr && current->engine->isBackCaptured();
}

void SokolRuntime::onEvent(const sapp_event* source, void* data) {
    SokolRuntime& runtime = *static_cast<SokolRuntime*>(data);
    if (runtime.engine == nullptr) {
        return;
    }

    // The back button leaves the app from its root screen, which on the Apple TV means the system takes the press, and goes back inside the app everywhere else.
    if (SokolEvents::isPlatformBack(*source)) {
        if (source->type == SAPP_EVENTTYPE_KEY_DOWN) {
            runtime.backCaptured = runtime.engine->isBackCaptured();
        }
        if (!runtime.backCaptured) {
            return;
        }
        sapp_consume_event();
    }
    if (const std::optional<Event> translated = SokolEvents::translate(*source)) {
        runtime.engine->handleEvent(*translated);
    }
}

#if defined(__ANDROID__)
bool SokolRuntime::onAndroidInput(const void* source) {
    return AndroidGamepads::handleEvent(source) || AndroidKeys::handleEvent(source);
}

void SokolRuntime::reportBackCapture() {
    const bool captured = engine->isBackCaptured() || AndroidTextInput::isEditing();
    if (captured == backReported) {
        return;
    }
    backReported = captured;
    JavaBridge::captureBack(captured);
}
#endif

void SokolRuntime::onCleanup(void* data) {
    static_cast<SokolRuntime*>(data)->close();
    getProcess().current.reset();
    Services::shutdown();
}

void SokolRuntime::launch() {
    App app = std::exchange(pending, {});
    playing = app.playing;
    paused = false;
    try {
        engine = std::make_unique<core::Engine>(host, std::move(app.package), std::move(app.config), std::move(app.application));
    } catch (const std::exception& error) {
        // Without an engine there is no error screen, so the failure goes to the log and the page. A desktop app closes, while the web runtime waits for the next package.
        core::Log::error("The engine could not start: {}", error.what());
        Services::reportError(lua::Error(error.what()).toJson());
#if !defined(__EMSCRIPTEN__)
        sapp_quit();
#endif
        return;
    }
    engine->start();
    if (online) {
        engine->handleEvent({.type = Event::Type::NetworkChanged, .online = *online});
    }
#if defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStarted(engine->getConfig());
    }
#elif defined(__ANDROID__)
    JavaBridge::setAppRunning(true);
#elif defined(__APPLE__)
    AppleRuntime::setAppRunning(true);
#endif
}

void SokolRuntime::close() noexcept {
    if (engine == nullptr) {
        return;
    }
#if defined(__ANDROID__)
    // Native events wait in Java from here on, so the ones that come while the app stops reach the next app instead of none.
    JavaBridge::setAppRunning(false);
#elif defined(__APPLE__)
    AppleRuntime::setAppRunning(false);
#endif
    engine.reset();
#if defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStopped();
    }
#endif
}

// The typing of the plain keyboard reaches the app as the key and character events a physical keyboard would send.
void SokolRuntime::deliver(const Event& event) {
    if (event.type == Event::Type::NetworkChanged) {
        online = event.online;
    }
    if (engine == nullptr) {
        return;
    }

    const bool typed = event.type == Event::Type::TextEdited || event.type == Event::Type::TextAction;
    if (!typed || event.textEdit.field != TextInput::kKeyboardField) {
        engine->handleEvent(event);
        return;
    }
    for (const Event& key : host.translateKeyboard(event)) {
        engine->handleEvent(key);
    }
}

// The package of the last playing app stays around, so a restart after a stop brings that app back.
void SokolRuntime::replace(App app) {
    const bool running = engine != nullptr;
    close();
    if (app.playing) {
        package = app.package;
    }
    pending = std::move(app);

    // Before sokol_app initializes, which on the web waits for the GPU device, the new app simply replaces the one waiting to start.
    if (running) {
        host.setTitle(pending.config.window.title);
        launch();
    }
}

} // namespace haylen::platform

#if defined(__APPLE__)
// sokol_app leaves main to Apple apps, which call this after registering their native HaylenBridge handlers.
int haylen_main(int argc, char* argv[]) {
    return haylen::platform::SokolRuntime::run(argc, argv);
}
#else
sapp_desc sokol_main(int argc, char* argv[]) {
    return haylen::platform::SokolRuntime::describe(argc, argv);
}
#endif
