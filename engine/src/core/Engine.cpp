#include "haylen/core/Engine.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/EmbeddedFiles.hpp"
#include "core/EngineState.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/ProfileScope.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "lua/Environment.hpp"
#include "platform/native/NativeApi.hpp"
#include "plugins/BuiltInPlugins.hpp"

namespace haylen::core {

Engine::Engine(platform::Host& host, std::shared_ptr<io::Package> package, AppConfig config, std::unique_ptr<Application> application) {
    if (!package || !application) {
        throw std::invalid_argument("The engine needs a package and an application.");
    }

    state = std::make_unique<EngineState>(host, std::move(package), std::move(config), std::move(application));
    EngineState& current = *state;

    // The Varn runtime comes first because the job system, asset loading and Lua all run on it.
    current.runtime = std::make_unique<varn::runtime::Runtime>(std::vector<std::string>{"haylen", std::string(current.package->getName())}, 1);
    current.jobs = std::make_unique<JobSystem>(*current.runtime, [this](const std::string& message) { reportError(message); });
    current.graphics = std::make_unique<graphics::Device>(host.getGraphicsSetup());
    audio::Mixer::Setup audioSetup = host.getAudioSetup();
    audioSetup.session = current.config.audioSession;
    current.audio = std::make_unique<audio::Mixer>(audioSetup);
    // clang-format off
    current.storage = std::make_unique<storage::UserStorage>(host.getUserDataDirectory(current.config.identifier), [&host] { host.persistUserData(); });
    // clang-format on

    // Handlers that native libraries register through the C interface answer before the handlers of the platform.
    // clang-format off
    current.platform = std::make_unique<platform::Bridge>(
        [&host](std::uint64_t id, std::string_view method, std::string_view params) {
            if (!platform::NativeApi::dispatch(id, method, params)) {
                host.dispatchPlatformCall(id, method, params);
            }
        },
        [&host](std::uint64_t id, std::string_view method) {
            if (!platform::NativeApi::cancel(id, method)) {
                host.cancelPlatformCall(id);
            }
        });
    // clang-format on
    current.renderer = std::make_unique<graphics2d::Renderer>(*current.graphics, *current.jobs);
    current.assets = std::make_unique<assets::Manager>(*current.package, *current.jobs, *current.graphics, current.events);
    current.scenes = std::make_unique<SceneManager>(*this);

    const std::span<const std::uint8_t> font = EmbeddedFiles::getDefaultFont();
    current.defaultFont = std::make_shared<text::TrueTypeFont>(*current.graphics, std::vector<std::uint8_t>(font.begin(), font.end()));
    current.safeAreaSimulation = current.config.debug.safeArea;
    current.reservedInsets = host.getReservedInsets();
    current.viewport.update(host.getFramebufferSize(), current.config.designSize, current.config.scaling, getSafeAreaInsets());
    current.fullscreen = host.isFullscreen();
    current.windowPosition = host.getFrame().getPosition();
    current.orientation = host.getOrientation();
    current.safeRect = current.viewport.getSafeRect();
    plugins::BuiltInPlugins::registerAll(current.plugins);
}

Engine::~Engine() {
    stop();

    // Everything that holds Lua references lets go before the Lua state closes, including scenes that stop hooks requested and work still queued on the pools, and Lua and the worker pools go before the assets, plugins and GPU resources they still reference.
    EngineState& current = *state;
    current.timers.clear();
    current.tweens.clear();
    current.frameQueue.clear();
    current.events.clear();
    current.assets->cancelAll();
    current.scenes.reset();
    current.platform.reset();
    current.runtime->stop();
    current.jobs->discardQueued();
    current.runtime.reset();
    current.audio.reset();
    current.plugins.clear();
    current.assets.reset();
    current.defaultFont.reset();
    current.renderer.reset();
    current.graphics->collectGarbage();
    current.graphics.reset();
}

void Engine::activatePlugin(plugins::Plugin& plugin) {
    if (!state->started) {
        return;
    }
    plugin.start(*this);
    state->startedPlugins.push_back(&plugin);
    plugin.installLua(*this, getLuaState());
    state->events.emit(LifecycleEvent::kPluginStarted, {{"name", std::string(plugin.getName())}});
}

void Engine::start() {
    EngineState& current = *state;
    if (current.started) {
        return;
    }
    current.started = true;

    applyWindowOptions();

    // A plugin or an application that fails to start shows the error screen instead of taking the process down.
    try {
        lua::Environment::install(*this, getLuaState());
        for (plugins::Plugin* plugin : current.plugins.getAll()) {
            plugin->start(*this);
            current.startedPlugins.push_back(plugin);
            plugin->installLua(*this, getLuaState());
            current.events.emit(LifecycleEvent::kPluginStarted, {{"name", std::string(plugin->getName())}});
        }
        lua::Environment::checkModules(*this);
        current.application->start(*this);
        current.events.emit(LifecycleEvent::kAppStarted);
    } catch (const std::exception& exception) {
        reportError(exception);
    }
}

// Every start applies the window options of app.json again, so an app that restarts from its package gets the window it asks for. The position only applies when the window opens, so a restart leaves the window where the player moved it.
void Engine::applyWindowOptions() {
    EngineState& current = *state;
    const AppConfig::Window& window = current.config.window;
    platform::Host& host = current.host;
    host.setTransparent(window.transparent && host.canBeTransparent());
    host.setResizable(window.resizable);
    host.setDecorated(window.decorated);
    host.setAlwaysOnTop(window.alwaysOnTop);
    host.setShowInTaskbar(window.showInTaskbar);
    host.setFocusable(window.focusable);
    host.setMousePassthrough(window.mousePassthrough ? platform::Window::Passthrough::Whole : platform::Window::Passthrough::Off, {});
}

void Engine::stop() {
    EngineState& current = *state;
    if (!current.started) {
        return;
    }
    current.started = false;

    // Stopping runs app code that may fail like any other, and every part still stops so nothing holds Lua references once the Lua state closes.
    // clang-format off
    const auto attempt = [this](const auto& step) {
        try {
            step();
        } catch (const std::exception& exception) {
            reportError(exception);
        }
    };
    // clang-format on
    attempt([&current] { current.events.emit(LifecycleEvent::kAppStopping); });
    attempt([&current] { current.scenes->clear(); });
    attempt([this, &current] { current.application->stop(*this); });

    const std::vector<plugins::Plugin*> started = std::exchange(current.startedPlugins, {});
    for (auto plugin = started.rbegin(); plugin != started.rend(); ++plugin) {
        attempt([this, plugin] { (*plugin)->stop(*this); });
        attempt([&current, plugin] { current.events.emit(LifecycleEvent::kPluginStopped, {{"name", std::string((*plugin)->getName())}}); });
    }
}

void Engine::frame(double frameSeconds) {
    EngineState& current = *state;
    if (!current.running) {
        return;
    }

    // Native UI that covered or uncovered the app since the last frame changes its state before the frame decides whether it is halted.
    try {
        applyCover();
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    // A halted app lets no time pass, and an app in the background draws nothing, so it does no GPU work.
    const bool halted = isHalted();
    const bool hidden = current.appState == AppState::Background;
    remapViewport();
    current.host.pollGamepads(current.gamepads);
    current.input.updateGamepads(current.gamepads);
    current.actions.update(current.input, current.virtualInput, halted || current.scenes->isInputBlocked());
    current.clock.advance(halted ? 0.0 : frameSeconds);
    const auto delta = static_cast<float>(current.clock.getDelta());
    current.input.updateTouchDurations(static_cast<float>(current.clock.getUnscaledDelta()));
    current.gestures.update(current.input, static_cast<float>(current.clock.getUnscaledDelta()));
    const std::vector<plugins::Plugin*> all = current.plugins.getAll();
    debug::Profiler& profiler = current.profiler;
    profiler.beginFrame();

    try {
        // Window and gamepad changes, asynchronous results and platform replies arrive before the app updates, and their callbacks may fail like any app code.
        publishDeviceChanges();

        // Decoded assets create their GPU resources within the upload budget of the frame, so a burst of loads never stalls it, and before the event loop runs, so the promises they settle resume their coroutines in this frame. An app in the background creates none.
        if (!hidden) {
            current.assets->finalizePending();
        }
        {
            const debug::ProfileScope scope(profiler, "scripts");
            current.runtime->poll();
            current.platform->pump();
        }

        if (!hidden) {
            for (plugins::Plugin* plugin : all) {
                plugin->beginFrame(*this, delta);
            }
        }

        if (!current.errorScreen && !halted) {
            const auto step = static_cast<float>(current.clock.getFixedStep());
            while (current.clock.consumeFixedStep()) {
                const debug::ProfileScope scope(profiler, "fixedUpdate");
                current.scenes->fixedUpdate(current.clock);
                current.tweens.fixedUpdate(current.clock);
                for (plugins::Plugin* plugin : all) {
                    plugin->fixedUpdate(*this, step);
                }
            }

            const debug::ProfileScope scope(profiler, "update");
            current.timers.update(current.clock);
            current.tweens.update(current.clock);
            for (plugins::Plugin* plugin : all) {
                plugin->update(*this, delta);
            }
            current.scenes->update(current.clock);
        }
        current.audio->update(static_cast<float>(current.clock.getUnscaledDelta()));
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    if (!hidden) {
        render(all);
    }

    // Deferred signal slots and queued events run last, after everything the frame did.
    try {
        current.frameQueue.flush();
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    current.graphics->collectGarbage();
    current.input.endFrame();
    profiler.endFrame();
}

// Gamepads that are connected when the app starts are announced on its first frame.
void Engine::publishDeviceChanges() {
    EngineState& current = *state;
    const bool fullscreen = current.host.isFullscreen();
    if (fullscreen != current.fullscreen) {
        current.fullscreen = fullscreen;
        current.events.emit(LifecycleEvent::kWindowFullscreenChanged, {{"fullscreen", fullscreen}});
    }

    const platform::Orientation orientation = current.host.getOrientation();
    if (orientation != current.orientation) {
        current.orientation = orientation;
        current.events.emit(LifecycleEvent::kWindowOrientationChanged, {{"orientation", platform::Window::orientationName(orientation)}});
    }

    const math::Rect safe = current.viewport.getSafeRect();
    if (safe != current.safeRect) {
        current.safeRect = safe;
        current.events.emit(LifecycleEvent::kWindowSafeAreaChanged, {{"x", JsonNumber::fromFloat(safe.x)}, {"y", JsonNumber::fromFloat(safe.y)}, {"width", JsonNumber::fromFloat(safe.width)}, {"height", JsonNumber::fromFloat(safe.height)}});
    }

    // Gamepads count from one in the event, as haylen.input counts them.
    for (std::size_t index = 0; index < current.gamepads.size(); ++index) {
        const auto& gamepad = current.gamepads[index];
        std::optional<std::string>& known = current.gamepadNames[index];
        if (gamepad.connected == known.has_value()) {
            continue;
        }
        const std::string name = gamepad.connected ? gamepad.name : *known;
        known = gamepad.connected ? std::optional<std::string>(gamepad.name) : std::nullopt;
        current.events.emit(gamepad.connected ? LifecycleEvent::kGamepadConnected : LifecycleEvent::kGamepadDisconnected, {{"gamepad", index + 1}, {"name", name}});
    }
}

void Engine::render(const std::vector<plugins::Plugin*>& all) {
    EngineState& current = *state;
    debug::Profiler& profiler = current.profiler;

    // Rendering records every canvas and submits the whole frame at the end.
    current.renderer->beginFrame(current.viewport, current.config.clearColor);
    try {
        const debug::ProfileScope scope(profiler, "render");
        if (!current.errorScreen) {
            renderScenes(all);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    if (current.errorScreen) {
        current.errorScreen->render();
    }
    try {
        for (plugins::Plugin* plugin : all) {
            plugin->renderOverlay(*this);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    try {
        const debug::ProfileScope scope(profiler, "submit");
        current.renderer->endFrame(current.host.getFrameTarget());
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    try {
        for (plugins::Plugin* plugin : all) {
            plugin->endFrame(*this);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
}

// Each view renders its scenes, with the drawing of the plugins in the current view. During a transition the views before the last one go into their own images, and the last one draws the effect on the screen before its own scenes.
void Engine::renderScenes(const std::vector<plugins::Plugin*>& all) {
    EngineState& current = *state;
    graphics2d::Renderer& renderer = *current.renderer;
    SceneManager& scenes = *current.scenes;
    for (const SceneManager::View& view : scenes.getViews()) {
        if (view.target.isValid()) {
            renderer.beginCapture(view.target, current.config.clearColor);
        }
        if (view.effect) {
            scenes.renderTransition(renderer, view);
        }
        scenes.render(view);
        if (view.current) {
            for (plugins::Plugin* plugin : all) {
                plugin->render(*this);
            }
        }
        scenes.renderUi(view);
        if (view.current) {
            for (plugins::Plugin* plugin : all) {
                plugin->renderUi(*this);
            }
        }
        if (view.target.isValid()) {
            renderer.endCapture();
        }
    }
}

void Engine::handleEvent(const platform::Event& event) {
    EngineState& current = *state;
    current.input.handleEvent(event, current.viewport);

    // The error screen takes the input of the stopped app, so its actions work even when a plugin fails on the same event.
    if (current.errorScreen) {
        current.errorScreen->handleEvent(event);
    }
    try {
        switch (event.type) {
        case platform::Event::Type::Resized: {
            resized.emit();
            const math::Vec2 size = current.host.getFramebufferSize();
            current.events.emit(LifecycleEvent::kWindowResized, {{"width", JsonNumber::fromFloat(size.x)}, {"height", JsonNumber::fromFloat(size.y)}});
            break;
        }
        case platform::Event::Type::FocusLost:
            current.focused = false;
            current.events.emit(LifecycleEvent::kWindowFocusLost);
            refreshForegroundState();
            break;
        case platform::Event::Type::FocusGained:
            current.focused = true;
            current.events.emit(LifecycleEvent::kWindowFocusGained);
            refreshForegroundState();
            break;
        case platform::Event::Type::Suspended:
            setAppState(AppState::Background);
            break;
        case platform::Event::Type::Resumed:
            setAppState(current.interrupted || current.covered ? AppState::Inactive : AppState::Active);
            break;
        case platform::Event::Type::InterruptionBegan:
            // An interruption of the system, such as a phone call or another app taking the audio focus, makes a foreground app inactive until it ends.
            current.interrupted = true;
            refreshForegroundState();
            break;
        case platform::Event::Type::InterruptionEnded:
            current.interrupted = false;
            refreshForegroundState();
            break;
        case platform::Event::Type::KeyboardChanged:
            publishKeyboard(event.keyboardFrame);
            break;
        case platform::Event::Type::WindowMoved: {
            // Platforms report every step of a move, and the app hears each new position once.
            const math::Vec2 position = current.host.getFrame().getPosition();
            if (position != current.windowPosition) {
                current.windowPosition = position;
                current.events.emit(LifecycleEvent::kWindowMoved, {{"x", JsonNumber::fromFloat(position.x)}, {"y", JsonNumber::fromFloat(position.y)}});
            }
            break;
        }
        case platform::Event::Type::MonitorsChanged:
            current.events.emit(LifecycleEvent::kWindowMonitorsChanged);
            break;
        case platform::Event::Type::NetworkChanged: {
            // The first report publishes the state the app starts in, and later reports publish only changes.
            const NetworkState reported = event.online ? NetworkState::Online : NetworkState::Offline;
            if (reported != current.network) {
                current.network = reported;
                current.events.emit(event.online ? LifecycleEvent::kNetworkOnline : LifecycleEvent::kNetworkOffline);
            }
            break;
        }
        case platform::Event::Type::QuitRequested:
            quitRequested.emit();
            current.events.emit(LifecycleEvent::kAppQuitRequested);
            break;
        case platform::Event::Type::LowMemory:
            current.assets->releaseUnused();
            lowMemory.emit();
            current.events.emit(LifecycleEvent::kAppLowMemory);
            break;
        default:
            break;
        }

        for (plugins::Plugin* plugin : current.plugins.getAll()) {
            plugin->event(*this, event);
        }
        if (!current.errorScreen) {
            current.scenes->event(event);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
}

// The keyboard frame reaches the app in design units, like the safe area. Every empty frame is the same hidden keyboard, wherever the platform places it.
void Engine::publishKeyboard(const math::Rect& value) {
    EngineState& current = *state;
    const math::Rect frame = value.isEmpty() ? math::Rect{} : value;
    if (frame == current.keyboardFrame) {
        return;
    }
    current.keyboardFrame = frame;
    if (frame.isEmpty()) {
        current.events.emit(LifecycleEvent::kKeyboardHidden);
        return;
    }
    const math::Rect area = math::Rect::fromMinMax(current.viewport.toDesign(frame.getMin()), current.viewport.toDesign(frame.getMax()));
    current.events.emit(LifecycleEvent::kKeyboardShown, {{"x", JsonNumber::fromFloat(area.x)}, {"y", JsonNumber::fromFloat(area.y)}, {"width", JsonNumber::fromFloat(area.width)}, {"height", JsonNumber::fromFloat(area.height)}});
}

void Engine::setAppState(AppState value) {
    EngineState& current = *state;
    if (current.appState == value) {
        return;
    }
    const AppState previous = current.appState;
    const bool wasHalted = isHalted();
    current.appState = value;

    // Leaving the foreground releases held input, including on-screen controls, and suspends audio, and coming back skips the time the app was away.
    if (value != AppState::Active) {
        current.input.releaseAll();
        current.gestures.cancel();
        current.virtualInput.clear();
    }
    if (value == AppState::Background) {
        current.audio->suspend();
    }
    if (previous == AppState::Background) {
        current.audio->resume();
        current.clock.skipNextDelta();
    }
    if (wasHalted && !isHalted()) {
        current.clock.skipNextDelta();
    }
    applyStateMute();

    appStateChanged.emit(value);
    switch (value) {
    case AppState::Active:
        current.events.emit(LifecycleEvent::kAppActive);
        break;
    case AppState::Inactive:
        current.events.emit(LifecycleEvent::kAppInactive);
        break;
    case AppState::Background:
        current.events.emit(LifecycleEvent::kAppBackground);

        // The platform may end an app in the background without warning, so what the app just saved becomes durable now.
        current.storage->flush();
        break;
    }
}

// An app in the foreground is active only while its window has the focus, no interruption of the system holds it and no native UI of a plugin covers it.
void Engine::refreshForegroundState() {
    const EngineState& current = *state;
    if (current.appState == AppState::Background) {
        return;
    }
    setAppState(current.focused && !current.interrupted && !current.covered ? AppState::Active : AppState::Inactive);
}

// Covered content must never play under native UI, so a cover halts and mutes the app whatever the lifecycle options say. The app state changes last, because its listeners may fail.
void Engine::applyCover() {
    EngineState& current = *state;
    const bool covered = current.host.isAppCovered();
    if (covered == current.covered) {
        return;
    }
    const bool wasHalted = isHalted();
    current.covered = covered;
    if (wasHalted && !isHalted()) {
        current.clock.skipNextDelta();
    }
    applyStateMute();
    refreshForegroundState();
}

void Engine::applyStateMute() {
    EngineState& current = *state;
    const bool mute = current.covered || (current.lifecycle.muteOnFocusLoss && current.appState != AppState::Active);
    if (mute == current.stateMuted) {
        return;
    }
    if (mute) {
        current.mutedBeforeState = current.audio->isBusMuted("master");
        current.audio->setBusMuted("master", true);
    } else {
        current.audio->setBusMuted("master", current.mutedBeforeState);
    }
    current.stateMuted = mute;
}

void Engine::setPaused(bool value) {
    EngineState& current = *state;
    if (current.clock.isPaused() == value) {
        return;
    }
    current.clock.setPaused(value);
    current.scenes->notifyPauseChange(value);
    pausedChanged.emit(value);
    current.events.emit(value ? LifecycleEvent::kPaused : LifecycleEvent::kUnpaused);
}

bool Engine::isPaused() const noexcept {
    return state->clock.isPaused();
}

Engine::AppState Engine::getAppState() const noexcept {
    return state->appState;
}

Engine::NetworkState Engine::getNetworkState() const noexcept {
    return state->network;
}

bool Engine::isAppCovered() const noexcept {
    return state->covered;
}

bool Engine::isHalted() const noexcept {
    const EngineState& current = *state;
    return current.covered || (current.appState == AppState::Background && current.lifecycle.pauseOnBackground) || (current.appState == AppState::Inactive && current.lifecycle.pauseOnFocusLoss);
}

const AppConfig::Lifecycle& Engine::getLifecycle() const noexcept {
    return state->lifecycle;
}

void Engine::setLifecycle(const AppConfig::Lifecycle& value) {
    EngineState& current = *state;
    const bool wasHalted = isHalted();
    current.lifecycle = value;
    if (wasHalted && !isHalted()) {
        current.clock.skipNextDelta();
    }
    applyStateMute();
}

void Engine::setScaling(graphics::Viewport::ScalingPolicy value) {
    state->config.scaling = value;
    remapViewport();
}

void Engine::setDesignSize(math::Vec2 value) {
    if (!(value.x > 0.0F) || !(value.y > 0.0F)) {
        throw std::invalid_argument("The design size needs a positive width and height.");
    }
    state->config.designSize = value;
    remapViewport();
}

// Input positions live in design units, so they move with the viewport whenever it maps the screen differently, such as after a resize or a rotation.
void Engine::remapViewport() {
    EngineState& current = *state;
    const graphics::Viewport previous = current.viewport;
    current.reservedInsets = current.host.getReservedInsets();
    current.viewport.update(current.host.getFramebufferSize(), current.config.designSize, current.config.scaling, getSafeAreaInsets());
    if (current.viewport.getPixelRect() != previous.getPixelRect() || current.viewport.getVisibleRect() != previous.getVisibleRect()) {
        current.input.followViewport(previous, current.viewport);
    }
}

void Engine::setSafeAreaSimulation(std::optional<platform::SafeAreaSimulation> value) {
    EngineState& current = *state;
    current.safeAreaSimulation = std::move(value);
    remapViewport();
}

const std::optional<platform::SafeAreaSimulation>& Engine::getSafeAreaSimulation() const noexcept {
    return state->safeAreaSimulation;
}

// The screen edges that native views reserve widen the safe area of the device, or the simulated one, edge by edge.
math::Insets Engine::getSafeAreaInsets() const {
    const EngineState& current = *state;
    const math::Insets device = current.safeAreaSimulation ? current.safeAreaSimulation->getInsets(current.host.getFramebufferSize(), current.host.getDpiScale()) : current.host.getSafeAreaInsets();
    const math::Insets& reserved = current.reservedInsets;
    return {.left = std::max(device.left, reserved.left), .top = std::max(device.top, reserved.top), .right = std::max(device.right, reserved.right), .bottom = std::max(device.bottom, reserved.bottom)};
}

const math::Insets& Engine::getReservedInsets() const noexcept {
    return state->reservedInsets;
}

std::vector<platform::AppPlugin> Engine::getAppPlugins() const {
    const EngineState& current = *state;
    const std::vector<std::string> loaded = getNativePlugins();
    std::vector<platform::AppPlugin> all;
    for (const auto& [id, values] : current.config.plugins.items()) {
        platform::AppPlugin plugin = platform::AppPlugin::read(*current.package, id, values);
        plugin.native = std::ranges::binary_search(loaded, id);
        all.push_back(std::move(plugin));
    }
    return all;
}

std::vector<std::string> Engine::getNativePlugins() const {
    std::vector<std::string> ids = state->host.getNativePlugins();
    const std::vector<std::string> declared = platform::NativeApi::getPlugins();
    ids.insert(ids.end(), declared.begin(), declared.end());
    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
    return ids;
}

void Engine::setBackLeavesApp(bool value) noexcept {
    state->backLeavesApp = value;
}

bool Engine::canBackLeaveApp() const noexcept {
    return state->backLeavesApp;
}

bool Engine::isBackCaptured() const {
    const EngineState& current = *state;
    const std::vector<plugins::Plugin*> all = current.plugins.getAll();
    return !current.backLeavesApp || std::ranges::any_of(all, [](const plugins::Plugin* plugin) { return plugin->isCapturingBack(); });
}

void Engine::reportError(const std::exception& exception) {
    EngineState& current = *state;
    if (current.errorScreen) {
        return;
    }

    const auto* scriptError = dynamic_cast<const lua::Error*>(&exception);
    current.errorScreen = std::make_unique<ErrorScreen>(*this, scriptError != nullptr ? *scriptError : lua::Error(exception.what()));
    Log::error("{}", current.errorScreen->getReport());
    current.host.reportError(current.errorScreen->getError().toJson());
    errorRaised.emit(current.errorScreen->getError());
}

void Engine::reportError(const std::string& message) {
    reportError(lua::Error(message));
}

const lua::Error* Engine::getError() const noexcept {
    return state->errorScreen ? &state->errorScreen->getError() : nullptr;
}

void Engine::requestRestart() noexcept {
    state->restartRequested = true;
}

bool Engine::isRestartRequested() const noexcept {
    return state->restartRequested;
}

void Engine::quit() {
    state->running = false;
    state->host.requestQuit();
}

bool Engine::isRunning() const noexcept {
    return state->running;
}

const AppConfig& Engine::getConfig() const noexcept {
    return state->config;
}

platform::Window& Engine::getWindow() noexcept {
    return state->host;
}

std::string_view Engine::getPlatformName() const noexcept {
    return state->host.getPlatformName();
}

io::Package& Engine::getPackage() noexcept {
    return *state->package;
}

storage::UserStorage& Engine::getStorage() noexcept {
    return *state->storage;
}

JobSystem& Engine::getJobs() noexcept {
    return *state->jobs;
}

TimerScheduler& Engine::getTimers() noexcept {
    return state->timers;
}

input::GestureRecognizer& Engine::getGestures() noexcept {
    return state->gestures;
}

debug::Profiler& Engine::getProfiler() noexcept {
    return state->profiler;
}

TweenManager& Engine::getTweens() noexcept {
    return state->tweens;
}

FrameClock& Engine::getClock() noexcept {
    return state->clock;
}

EventBus& Engine::getEvents() noexcept {
    return state->events;
}

FrameQueue& Engine::getFrameQueue() noexcept {
    return state->frameQueue;
}

graphics::Device& Engine::getGraphics() noexcept {
    return *state->graphics;
}

graphics2d::Renderer& Engine::getRenderer2D() noexcept {
    return *state->renderer;
}

audio::Mixer& Engine::getAudio() noexcept {
    return *state->audio;
}

graphics::Viewport& Engine::getViewport() noexcept {
    return state->viewport;
}

input::Input& Engine::getInput() noexcept {
    return state->input;
}

input::ActionMap& Engine::getActions() noexcept {
    return state->actions;
}

input::VirtualInput& Engine::getVirtualInput() noexcept {
    return state->virtualInput;
}

SceneManager& Engine::getScenes() noexcept {
    return *state->scenes;
}

assets::Manager& Engine::getAssets() noexcept {
    return *state->assets;
}

platform::Bridge& Engine::getPlatform() noexcept {
    return *state->platform;
}

plugins::PluginRegistry& Engine::getPlugins() noexcept {
    return state->plugins;
}

const std::shared_ptr<text::Font>& Engine::getDefaultFont() noexcept {
    return state->defaultFont;
}

varn::runtime::Runtime& Engine::getScriptRuntime() noexcept {
    return *state->runtime;
}

lua_State* Engine::getLuaState() noexcept {
    return state->runtime->luaState();
}

} // namespace haylen::core
