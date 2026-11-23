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
#include "haylen/assets/Manager.hpp"
#include "haylen/content/Bootstrap.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
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

    runtime.host->setLaunchArguments(std::move(options.arguments));

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

    return window.position->resolve(getCurrent().host->getDisplays(), {static_cast<float>(window.width), static_cast<float>(window.height)});
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

void Player::end() {
    getCurrent().host->closeWindow();
}

void Player::fail(std::string_view reason) {
    Player& runtime = getCurrent();
    runtime.close();
    core::Log::error("{}", reason);
    Services::reportError(core::AppError(std::string(reason)).toJson());
    if (runtime.host->keepsWindowAfterFailure()) {
        return;
    }

    getProcess().exitStatus = 1;
    runtime.host->closeWindow();
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

bool Player::isPlaying() noexcept {
    const Player& runtime = getCurrent();
    return runtime.engine != nullptr && runtime.playing;
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
    return session != nullptr && session->isHalted();
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

// Creates the application and reads the package configuration with the sections the application reads, turning any failure into an app that shows the error. A package that opened stays with the failed app, so a restart tries it again, and an app in development restarts as soon as its files change.
Player::App Player::load(const std::function<std::shared_ptr<io::Package>()>& open) {
    App app;
    try {
        app.package = withOverlay(open());
        app.application = core::Application::create();
        app.config = core::AppConfig::fromPackage(*app.package, app.application->getSections());
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
    const DevelopmentSession* session = host->getDevelopmentSession();
    const bool overlaid = host->overlaysEveryPackage() || (session != nullptr && session->isConnected());
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
    // The window shows the launch color of the app until the app draws into it, and a transparent window keeps the desktop showing through.
    const core::AppConfig& config = runtime.pending.config;
    if (!config.window.transparent) {
        Services::setWindowBackground(config.splash.background);
    }

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

    runtime.play(seconds);
    runtime.host->framePlayed(*runtime.engine);
    if (!runtime.engine->isRunning()) {
        if (!runtime.launchers.empty()) {
            runtime.returnToLauncher();
            return;
        }

        runtime.host->closeWindow();
        return;
    }

    runtime.host->frameFinished(*runtime.engine);

    // A restart or a launch happens between frames, where no engine code is on the stack.
    if (runtime.engine->isRestartRequested()) {
        restart();
    } else if (const std::optional<core::Engine::Launch>& request = runtime.engine->getLaunchRequest()) {
        runtime.launchPackage(*request);
    }
}

// The events that wait run before the frame, and the frame decides whether another app takes over.
void Player::play(double seconds) {
    host->setBackHeld(getProcess().backHeld.load());
    Services::updateWindow();

    if (const std::optional<Event::MemoryLevel> level = MemoryWarning::take()) {
        engine->handleEvent({.type = Event::Type::LowMemory, .memoryLevel = *level});
    }

    for (Event& event : takePostedEvents()) {
        host->preparePostedEvent(event);
        deliver(event);
    }

    for (int press = takeBackPresses(); press > 0 && engine->isRunning(); --press) {
        pressBack();
    }

    endSplashScreenWhenDue();
    engine->frame(seconds);
}

// The page hears what every batch of a development server did, as the web editor shows it.
void Player::enableDevelopment(std::optional<std::filesystem::path> folder, std::string server) {
    host->enableDevelopment(std::move(folder), std::move(server));
    host->developmentEnabled(*host->getDevelopmentSession());
}

// The app that launches keeps its package, its development session and its native access for its return, and the launched app opens like any package, in development with a session of its own when it names a development server, with native access only when both apps have it, and with the error screen when it cannot open.
void Player::launchPackage(const core::Engine::Launch& request) {
    const std::span<const std::string> arguments = host->getLaunchArguments();
    launchers.push_back({.package = package, .development = host->exchangeDevelopmentSession(nullptr), .nativeAccess = host->hasNativeAccess(), .arguments = {arguments.begin(), arguments.end()}});
    host->setLaunched(true);
    host->setLaunchArguments({});
    host->setNativeAccess(host->hasNativeAccess() && request.nativeAccess);

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
    host->setNativeAccess(launcher.nativeAccess);
    host->setLaunchArguments(std::move(launcher.arguments));
    // A hold of back that left the launched app ends with a release, which the app that starts again never hears.
    if (getProcess().backHeld.load()) {
        getProcess().backSwallowed.store(true);
    }

    const std::unique_ptr<DevelopmentSession> ended = host->exchangeDevelopmentSession(std::move(launcher.development));
    replace(load([&launcher] { return std::move(launcher.package); }));
}

// The launch screen of the platform stays until the app drew its first complete frame and the splash of haylen.json lasted its duration since the launch. It ends as the next frame starts, which leaves the GPU a whole frame to present the complete one, so a window that shows at that moment never shows the frame before it.
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
bool Player::isDrawing() noexcept {
    const std::unique_ptr<Player>& current = getProcess().current;
    return current != nullptr && current->engine != nullptr && current->engine->isDrawing();
}

void Player::requestRedraw() noexcept {
    const std::unique_ptr<Player>& current = getProcess().current;
    if (current != nullptr && current->engine != nullptr) {
        current->engine->requestRedraw();
    }
}

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
        // Without an engine there is no error screen, so the failure goes to the log and the page.
        core::Log::error("The engine could not start: {}", error.what());
        core::Json report = core::AppError(error.what()).toJson();
        report["phase"] = "startup";
        Services::reportError(report);
        host->appStartFailed(report);
    }

    if (engine == nullptr) {
        leaveFailedStart();
        return;
    }

    engine->start();

    for (const Event& event : reported.getReplay()) {
        engine->handleEvent(event);
    }

    host->appStarted(*engine, playing);
}

// An app whose engine could not start returns to the app that launched it. The app that runs first closes its desktop window, while the web runtime waits for the next package.
void Player::leaveFailedStart() {
    if (!launchers.empty()) {
        returnToLauncher();
        return;
    }

    if (host->keepsWindowAfterFailure()) {
        return;
    }

    getProcess().exitStatus = 1;
    host->closeWindow();
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

    host->appStopping();
    getProcess().exitStatus = engine->getExitStatus();
    engine.reset();

    host->appStopped(playing);
}

// The typing of the plain keyboard reaches the app as the key and character events a physical keyboard would send.
void Player::deliver(const Event& event) {
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
