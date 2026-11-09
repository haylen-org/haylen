#include "platform/Player.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "content/ReleasePackage.hpp"
#include "debug/Debugger.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/GraphicsApiOrder.hpp"
#include "platform/LaunchOptions.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/Services.hpp"
#include "platform/native/NativeApi.hpp"

// A port brings the services of its own platform, so the platform folders of the repository stay out of its builds even where its compiler targets their system, as a test port on a desktop does.
#if defined(__APPLE__) && !defined(HAYLEN_PORT)
#include "platform/apple/AppleRuntime.hpp"
#elif defined(__ANDROID__)
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/android/JavaBridge.hpp"
#elif defined(__EMSCRIPTEN__)
#include "platform/web/WebPage.hpp"
#elif defined(__linux__) && !defined(HAYLEN_PORT)
#include "platform/linux/LinuxAccessibility.hpp"
#endif

namespace haylen::platform {

Player::Player(std::unique_ptr<NativeHost> nativeHost) : host(std::move(nativeHost)) {}

int Player::getExitStatus() noexcept {
    return getProcess().exitStatus;
}

void Player::create(int argc, char* argv[], std::unique_ptr<NativeHost> nativeHost) {
    Services::initialize();
    Process& process = getProcess();
    process.current = std::make_unique<Player>(std::move(nativeHost));
    Player& runtime = *process.current;
    LaunchOptions options = LaunchOptions::parse(argc, argv);
#if defined(__ANDROID__)
    // Android starts apps without a command line, and a debuggable app takes the development server from the intent that launched it.
    if (std::string server = JavaBridge::getDevelopmentServer(); !server.empty()) {
        options.development = true;
        options.developmentServer = std::move(server);
    }
#endif
    // A release build carries the bootstrap of its app and plays its protected release, which never runs in development.
    if (options.development && content::Bootstrap::find() != nullptr) {
        core::Log::warning("The release build of the app ignores \"--dev\" and \"--dev-server\", since a release never runs in development.");
        options.development = false;
    }
    if (!options.package.empty() && content::Bootstrap::find() != nullptr) {
        core::Log::warning("The release build of the app ignores the package \"{}\", since it plays only its own release.", options.package);
        options.package.clear();
    }
    for (const std::string& folder : options.nativeFolders) {
        NativeLibraries::addSearchFolder(folder);
    }
    if (options.development) {
        // A player that a development server feeds takes the saved files from the server alone, so a save never reloads twice.
        std::error_code error;
        const bool watched = options.developmentServer.empty() && !options.package.empty() && std::filesystem::is_directory(options.package, error);
        runtime.enableDevelopment(watched ? std::optional<std::filesystem::path>(options.package) : std::nullopt, options.developmentServer);
    }
    // clang-format off
    runtime.pending = runtime.load([&options] {
        return options.package.empty() ? Services::openBundledPackage() : std::shared_ptr<io::Package>(io::Package::open(options.package));
    });
    // clang-format on
    runtime.package = runtime.pending.package;

    // A link that launched the process reaches the instance that runs already, or this app once it starts, which takes the links of later processes from then on.
    const std::string& identifier = runtime.pending.config.identifier;
    if (!options.link.empty()) {
        if (Services::forwardLink(identifier, options.link)) {
            process.launchForwarded = true;
            return;
        }
        LinkRelay::receive(std::move(options.link));
    }
    Services::listenForLinks(identifier);

    // The window opens with its desktop options already in effect, so a frameless window never shows a title bar for its first frame. Whether it can be transparent lasts for the whole run.
    const core::AppConfig::Window& window = runtime.pending.config.window;
    runtime.openingSize = {static_cast<float>(window.width), static_cast<float>(window.height)};
    runtime.graphicsApis = runtime.resolveGraphicsApis(options.graphicsApi, window.graphicsApis);
    runtime.host->prepare({.transparent = window.transparent, .resizable = window.resizable, .decorated = window.decorated, .alwaysOnTop = window.alwaysOnTop, .shownInTaskbar = window.showInTaskbar}, window.focusable);
}

bool Player::isLaunchForwarded() noexcept {
    return getProcess().launchForwarded;
}

const core::AppConfig& Player::getOpeningConfig() noexcept {
    return getCurrent().pending.config;
}

math::Vec2 Player::getOpeningSize() noexcept {
    return getCurrent().openingSize;
}

const std::vector<gpu::Device::Api>& Player::getGraphicsApis() noexcept {
    return getCurrent().graphicsApis;
}

std::optional<math::Rect> Player::getOpeningFrame() {
    const core::AppConfig::Window& window = getCurrent().pending.config.window;
    if (!window.position || !Services::hasDesktop()) {
        return std::nullopt;
    }
    return window.position->resolve(Services::getDisplays(), {static_cast<float>(window.width), static_cast<float>(window.height)});
}

void Player::restart(std::shared_ptr<io::Package> source) {
    restart([&source] { return std::move(source); });
}

void Player::restart(const std::function<std::shared_ptr<io::Package>()>& open) {
    Player& runtime = getCurrent();
    runtime.replace(runtime.load(open));
}

void Player::restart() {
    restart(getCurrent().package);
}

void Player::stage(std::shared_ptr<io::Package> source) {
    stop();
    getCurrent().package = std::move(source);
}

DevelopmentSession* Player::getDevelopmentSession() noexcept {
    return getCurrent().host->getDevelopmentSession();
}

io::OverlayPackage& Player::getOverlay() {
    auto* overlay = dynamic_cast<io::OverlayPackage*>(getCurrent().package.get());
    if (overlay == nullptr) {
        throw std::runtime_error("The app plays a protected release, whose files a page never edits.");
    }
    return *overlay;
}

void Player::stop() {
    getCurrent().replace({.package = std::make_shared<io::MemoryPackage>("stopped"), .config = {}, .application = std::make_unique<StoppedApplication>(), .playing = false});
}

void Player::fail(std::string_view reason) {
    Player& runtime = getCurrent();
    runtime.close();
    core::Log::error("{}", reason);
    Services::reportError(lua::Error(std::string(reason)).toJson());
#if !defined(__EMSCRIPTEN__)
    getProcess().exitStatus = 1;
    runtime.host->closeWindow();
#endif
}

// A page that pauses an app in a hidden tab, or resumes it there, changes nothing the app hears until the tab shows again.
void Player::setPaused(bool value) {
    Player& runtime = getCurrent();
    if (runtime.engine == nullptr) {
        return;
    }
    if (const std::optional<Event> event = runtime.reported.setPaused(value)) {
        runtime.engine->handleEvent(*event);
    }
}

bool Player::isPaused() noexcept {
    return getCurrent().reported.isPaused();
}

// Observers of the system outlive the player, such as those of GameController, so an event after the window closed goes nowhere.
void Player::handleEvent(const Event& event) {
    if (isHalted()) {
        postEvent(event);
        return;
    }
    if (const std::unique_ptr<Player>& current = getProcess().current) {
        current->deliver(event);
    }
}

bool Player::isHalted() noexcept {
    const std::unique_ptr<Player>& current = getProcess().current;
    if (current == nullptr) {
        return false;
    }
    const DevelopmentSession* session = current->host->getDevelopmentSession();
    return session != nullptr && session->getDebugger().isHalted();
}

void Player::postEvent(Event event) {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    process.posted.push_back(std::move(event));
}

std::vector<Event> Player::takePostedEvents() {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    return std::exchange(process.posted, {});
}

void Player::postBack() {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    ++process.backPresses;
}

bool Player::holdBack(bool pressed) {
    Process& process = getProcess();
    process.backHeld.store(pressed);
    return !pressed && process.backSwallowed.exchange(false);
}

int Player::takeBackPresses() {
    Process& process = getProcess();
    const std::scoped_lock lock(process.postedMutex);
    return std::exchange(process.backPresses, 0);
}

// Back reaches the app as escape, except that a launched app that does not take back quits to the app that launched it.
void Player::pressBack() {
    if (host->isLaunched() && !engine->isBackCaptured()) {
        engine->quit();
        return;
    }
    deliver({.type = Event::Type::KeyDown, .key = input::Key::Escape});
    deliver({.type = Event::Type::KeyUp, .key = input::Key::Escape});
}

Player::Process& Player::getProcess() noexcept {
    static Process& process = *new Process();
    return process;
}

Player& Player::getCurrent() noexcept {
    return *getProcess().current;
}

// Reads the package configuration and creates the application, turning any failure into an app that shows the error. A package that opened stays with the failed app, so a restart tries it again, and an app in development restarts as soon as its files change.
Player::App Player::load(const std::function<std::shared_ptr<io::Package>()>& open) {
    App app;
    try {
        app.package = withOverlay(open());
        app.config = core::AppConfig::fromPackage(*app.package);
        app.application = core::Application::create();
        app.application->configure(app.config);
    } catch (const std::exception& error) {
        core::Log::error("The app could not be loaded: {}", error.what());
        if (app.package == nullptr) {
            app.package = withOverlay(std::make_shared<io::MemoryPackage>("missing"));
        }
        app.config = {};
        app.application = std::make_unique<FailedApplication>(std::string("The app could not be loaded. ") + error.what());
    }
    return app;
}

// A graphics API that the command line or the web page names wrongly fails the app with the reason on its error screen, which opens through the order of the app.
std::vector<gpu::Device::Api> Player::resolveGraphicsApis(std::string_view requested, std::span<const gpu::Device::Api> configured) {
    try {
        return GraphicsApiOrder::resolve(requested, configured);
    } catch (const std::invalid_argument& error) {
        core::Log::error("The app could not be loaded: {}", error.what());
        pending.application = std::make_unique<FailedApplication>(std::string("The app could not be loaded. ") + error.what());
        return GraphicsApiOrder::resolve({}, configured);
    }
}

// The web plays every package under an overlay, which the page edits file by file, and so does an app that a development server sends files to. A protected release plays as it ships, since no file of it is ever replaced.
std::shared_ptr<io::Package> Player::withOverlay(std::shared_ptr<io::Package> source) const {
#if defined(__EMSCRIPTEN__)
    const bool overlaid = true;
#else
    const DevelopmentSession* session = host->getDevelopmentSession();
    const bool overlaid = session != nullptr && session->isConnected();
#endif
    if (!overlaid || std::dynamic_pointer_cast<io::OverlayPackage>(source) || std::dynamic_pointer_cast<content::ReleasePackage>(source)) {
        return source;
    }
    return std::make_shared<io::OverlayPackage>(std::move(source));
}

// Native libraries open windows of their own over the window of the app, which exists from here on.
void Player::initialize() {
    Services::watchWindow();
    NativeApi::setWindow(Services::getNativeWindow());
    Player& runtime = getCurrent();
    runtime.initialized = true;
    runtime.launch();
}

void Player::frame(double seconds) {
    Player& runtime = getCurrent();
    if (runtime.reported.isPaused()) {
        return;
    }
    if (runtime.waiting) {
        runtime.launchWhenReady();
    }
    if (runtime.engine == nullptr) {
        return;
    }
    runtime.host->setBackHeld(getProcess().backHeld.load());
    Services::updateWindow();
    if (const std::optional<Event::MemoryLevel> level = MemoryWarning::take()) {
        runtime.engine->handleEvent({.type = Event::Type::LowMemory, .memoryLevel = *level});
    }
    for (Event& event : takePostedEvents()) {
#if defined(__ANDROID__)
        // The on-screen keyboard of Android arrives from the UI thread in window pixels.
        if (event.type == Event::Type::KeyboardChanged) {
            event.keyboardFrame = AndroidActivity::toFramebuffer(event.keyboardFrame);
        }
#endif
        runtime.deliver(event);
    }
    for (int press = takeBackPresses(); press > 0 && runtime.engine->isRunning(); --press) {
        runtime.pressBack();
    }
    runtime.endSplashScreenWhenDue();
    runtime.engine->frame(seconds);
#if defined(__ANDROID__)
    runtime.reportBackCapture();
#endif
    if (!runtime.engine->isRunning()) {
        if (!runtime.launchers.empty()) {
            runtime.returnToLauncher();
            return;
        }
        runtime.host->closeWindow();
        return;
    }
#if defined(__EMSCRIPTEN__)
    WebPage::reportFrame(*runtime.engine);
#endif

    // A restart or a launch happens between frames, where no engine code is on the stack.
    if (runtime.engine->isRestartRequested()) {
        restart();
    } else if (const std::optional<core::Engine::Launch>& request = runtime.engine->getLaunchRequest()) {
        runtime.launchPackage(*request);
    }
}

// The page hears what every batch of a development server did, as the web editor shows it.
void Player::enableDevelopment(std::optional<std::filesystem::path> folder, std::string server) {
    host->enableDevelopment(std::move(folder), std::move(server));
#if defined(__EMSCRIPTEN__)
    host->getDevelopmentSession()->addReporter(&WebPage::reportReloaded);
#endif
}

// The app that launches keeps its package and its development session for its return, and the launched app opens like any package, in development with a session of its own when it names a development server, and with the error screen when it cannot open.
void Player::launchPackage(const core::Engine::Launch& request) {
    launchers.push_back({.package = package, .development = host->exchangeDevelopmentSession(nullptr)});
    host->setLaunched(true);
    if (!request.developmentServer.empty()) {
        enableDevelopment(std::nullopt, request.developmentServer);
    }
    replace(load([&request] { return std::shared_ptr<io::Package>(io::Package::open(request.package)); }));
}

// The session of the launched app lasts until its engine closed, since the plugins of that engine hold it.
void Player::returnToLauncher() {
    Launcher launcher = std::move(launchers.back());
    launchers.pop_back();
    host->setLaunched(!launchers.empty());
    // A hold of back that left the launched app ends with a release, which the app that starts again never hears.
    if (getProcess().backHeld.load()) {
        getProcess().backSwallowed.store(true);
    }
    const std::unique_ptr<DevelopmentSession> ended = host->exchangeDevelopmentSession(std::move(launcher.development));
    replace(load([&launcher] { return std::move(launcher.package); }));
}

// The launch screen of the platform stays until the app drew its first complete frame and the splash of app.json lasted its duration since the launch. It ends as the next frame starts, which leaves the GPU a whole frame to present the complete one, so a window that shows at that moment never shows the frame before it.
void Player::endSplashScreenWhenDue() {
    if (splashEnded || !engine->isFrameComplete()) {
        return;
    }
    const core::AppConfig::Splash& splash = engine->getConfig().splash;
    if (std::chrono::steady_clock::now() - launched < std::chrono::duration<float>(splash.duration)) {
        return;
    }
    splashEnded = true;
    host->endSplashScreen(splash.fadeOut);
}

bool Player::isBackCaptured() {
    const std::unique_ptr<Player>& current = getProcess().current;
    return current != nullptr && current->engine != nullptr && current->engine->isBackCaptured();
}

gpu::Device* Player::getGpuDevice() noexcept {
    const std::unique_ptr<Player>& current = getProcess().current;
    if (current == nullptr || current->engine == nullptr) {
        return nullptr;
    }
    return &current->engine->getGraphics().getGpu();
}

// The back button leaves the app from its root screen, which on the Apple TV means the system takes the press, and goes back inside the app everywhere else. A launched app leaves to the app that launched it instead.
Player::Back Player::routeBack(bool pressed) {
    Player& runtime = getCurrent();
    if (runtime.engine == nullptr) {
        return Back::System;
    }
    if (pressed) {
        runtime.backCaptured = runtime.engine->isBackCaptured();
    }
    if (runtime.backCaptured) {
        return Back::App;
    }
    if (!runtime.host->isLaunched()) {
        return Back::System;
    }
    if (!pressed) {
        runtime.engine->quit();
    }
    return Back::Player;
}

#if defined(__ANDROID__)
void Player::reportBackCapture() {
    const bool captured = engine->isBackCaptured() || AndroidTextInput::isEditing() || host->isLaunched() || getProcess().backHeld.load();
    if (captured == backReported) {
        return;
    }
    backReported = captured;
    JavaBridge::captureBack(captured);
}
#endif

void Player::cleanup() {
    getCurrent().close();
    getProcess().current.reset();
    Services::shutdown();
}

void Player::launch() {
    App app = std::exchange(pending, {});
    playing = app.playing;
    (void)reported.setPaused(false);
    if (DevelopmentSession* session = host->getDevelopmentSession()) {
        session->setOverlay(std::dynamic_pointer_cast<io::OverlayPackage>(app.package));
    }
    try {
        engine = std::make_unique<core::Engine>(*host, std::move(app.package), std::move(app.config), std::move(app.application));
    } catch (const std::exception& error) {
        // Without an engine there is no error screen, so the failure goes to the log and the page. A desktop app closes, while the web runtime waits for the next package.
        core::Log::error("The engine could not start: {}", error.what());
        Services::reportError(lua::Error(error.what()).toJson());
#if !defined(__EMSCRIPTEN__)
        getProcess().exitStatus = 1;
        host->closeWindow();
#endif
        return;
    }
    engine->start();
    for (const Event& event : reported.getReplay()) {
        engine->handleEvent(event);
    }
#if defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStarted(engine->getConfig());
    }
#elif defined(__ANDROID__)
    JavaBridge::setAppRunning(true);
#elif defined(__APPLE__) && !defined(HAYLEN_PORT)
    AppleRuntime::setAppRunning(true);
#endif
}

void Player::launchWhenReady() {
    if (!host->prepareGpuDevice()) {
        return;
    }
    waiting = false;
    launch();
}

void Player::close() noexcept {
    if (engine == nullptr) {
        return;
    }
#if defined(__ANDROID__)
    // Native events wait in Java from here on, so the ones that come while the app stops reach the next app instead of none.
    JavaBridge::setAppRunning(false);
#elif defined(__APPLE__) && !defined(HAYLEN_PORT)
    AppleRuntime::setAppRunning(false);
#endif
    getProcess().exitStatus = engine->getExitStatus();
    engine.reset();
#if defined(__EMSCRIPTEN__)
    if (playing) {
        WebPage::reportStopped();
    }
#endif
}

// The typing of the plain keyboard reaches the app as the key and character events a physical keyboard would send.
void Player::deliver(const Event& event) {
#if defined(__ANDROID__)
    // Android sends the axes of controllers only to the window with the focus, so a stick released while another window has it would keep its last value.
    if (event.type == Event::Type::FocusLost) {
        AndroidGamepads::releaseAxes();
    }
#elif defined(__linux__) && !defined(HAYLEN_PORT)
    // Screen readers follow the focus of the window that is active, which the window tells them as it gains and loses the focus.
    if (event.type == Event::Type::FocusGained || event.type == Event::Type::FocusLost) {
        LinuxAccessibility::setActive(event.type == Event::Type::FocusGained);
    }
#endif
    const bool heard = reported.record(event);
    if (engine == nullptr || !heard) {
        return;
    }

    const bool typed = event.type == Event::Type::TextEdited || event.type == Event::Type::TextAction;
    if (!typed || event.textEdit.field != TextInput::kKeyboardField) {
        engine->handleEvent(event);
        return;
    }
    for (const Event& key : host->translateKeyboard(event)) {
        engine->handleEvent(key);
    }
}

// The package of the last playing app stays around, so a restart after a stop brings that app back.
void Player::replace(App app) {
    close();
    if (app.playing) {
        package = app.package;
    }
    pending = std::move(app);

    // Before the window opens, which on the web waits for the GPU device, the new app simply replaces the one waiting to start. Once it opened, the new app starts as soon as the platform can create its GPU device, which WebGPU hands over a few frames later in browsers, also after an app whose engine could not start, which the web page replaces this way.
    if (initialized) {
        host->setTitle(pending.config.window.title);
        waiting = true;
        launchWhenReady();
    }
}

} // namespace haylen::platform
