#include "haylen/core/Engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "core/EngineState.hpp"
#include "haylen/audio/Limiter.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/ProfileScope.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "lua/Environment.hpp"
#include "platform/ScreenRelay.hpp"
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
    audioSetup.latency = current.config.audioOutput.latency;
    current.audio = std::make_unique<audio::Mixer>(audioSetup, *current.jobs);
    if (current.config.audioOutput.limiter) {
        current.audio->addBusEffect("master", std::make_shared<audio::Limiter>(audio::Limiter::Settings{}));
    }
    // clang-format off
    current.storage = std::make_unique<storage::UserStorage>(host.getUserDataDirectory(current.config.identifier), [&host] { host.persistUserData(); });
    // clang-format on

    // Handlers that native libraries register through the C interface answer before the handlers of the platform.
    // clang-format off
    current.platform = std::make_unique<platform::Bridge>(
        [&host](std::uint64_t id, std::string_view method, std::string_view params, std::span<const std::vector<std::byte>> buffers) {
            if (!platform::NativeApi::dispatch(id, method, params, buffers)) {
                host.dispatchPlatformCall(id, method, params, buffers);
            }
        },
        [&host](std::uint64_t id, std::string_view method) {
            if (!platform::NativeApi::cancel(id, method)) {
                host.cancelPlatformCall(id);
            }
        });
    // clang-format on
    current.system = std::make_unique<platform::System>(host, current.events, current.graphics->getAdapterName());
    current.mediaSession = std::make_unique<platform::MediaSession>(host, current.events);
    current.accessibility = std::make_unique<platform::Accessibility>(host, current.events);
    current.dialogs = std::make_unique<platform::Dialogs>(host, current.storage->getRoot() / "tmp" / "dialogs");
    current.screens = std::make_unique<platform::Screens>(host, *current.platform, [this] { return getAppState(); });
    current.renderer = std::make_unique<graphics2d::Renderer>(*current.graphics, *current.jobs);
    current.assets = std::make_unique<assets::Manager>(*current.package, *current.jobs, *current.graphics, current.events);
    current.content = std::make_unique<content::ContentManager>(*this, current.package, current.config);

    // Assets whose bytes are not on the device come down through the content manager before the asset manager reads them.
    content::ContentManager& contentManager = *current.content;
    // clang-format off
    current.assets->setAvailability({
        .isLocal = [&contentManager](std::string_view path) { return contentManager.isLocal(path); },
        .acquire = [&contentManager](std::vector<std::string> paths, assets::Manager::GroupProgress onProgress, std::function<void(std::string)> done) {
            contentManager.acquire(paths, [onProgress = std::move(onProgress)](const content::ContentManager::Progress& progress) {
                if (onProgress) {
                    onProgress(progress.totalBytes == 0 ? 1.0F : static_cast<float>(progress.receivedBytes) / static_cast<float>(progress.totalBytes));
                }
            }, [done = std::move(done)](std::optional<content::Error> error) { done(error ? error->what() : std::string()); });
        },
    });
    // clang-format on
    current.scenes = std::make_unique<SceneManager>(*this);

    const std::span<const std::uint8_t> font = EmbeddedFiles::getDefaultFont();
    current.defaultFont = std::make_shared<text::TrueTypeFont>(*current.graphics, std::vector<std::uint8_t>(font.begin(), font.end()));
    current.safeAreaSimulation = current.config.debug.safeArea;
    current.foldSimulation = current.config.debug.fold;
    current.pointerEmulation.setMouseAsTouch(current.config.input.mouseAsTouch);
    current.pointerEmulation.setTouchAsMouse(current.config.input.touchAsMouse);
    current.input.setScrollSpeed(current.config.input.scrollSpeed);
    current.reservedInsets = host.getReservedInsets();
    current.viewport.update(host.getFramebufferSize(), current.config.designSize, current.config.scaling, getSafeAreaInsets(), host.getDpiScale());
    current.fullscreen = host.isFullscreen();
    current.windowPosition = host.getFrame().getPosition();
    current.orientation = host.getOrientation();
    current.safeRect = current.viewport.getSafeRect();
    current.fold = getFold();
    plugins::BuiltInPlugins::registerAll(current.plugins, host);
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
    current.content.reset();
    current.scenes.reset();
    current.screens.reset();
    current.platform.reset();
    current.dialogs.reset();
    current.accessibility.reset();
    current.system->stopRumbles();
    current.system.reset();
    current.mediaSession.reset();
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
    const PhaseScope scope(*this, Phase::Lifecycle);
    try {
        lua::Environment::install(*this, getLuaState());
        for (plugins::Plugin* plugin : current.plugins.getAll()) {
            plugin->start(*this);
            current.startedPlugins.push_back(plugin);
            plugin->installLua(*this, getLuaState());
            current.events.emit(LifecycleEvent::kPluginStarted, {{"name", std::string(plugin->getName())}});
        }
        lua::Environment::checkModules(*this);
        if (isStartHeld()) {
            current.applicationPending = true;
            return;
        }
        startApplication();
    } catch (const std::exception& exception) {
        reportError(exception);
    }
}

void Engine::startApplication() {
    EngineState& current = *state;
    current.applicationPending = false;
    current.application->start(*this);
    current.events.emit(LifecycleEvent::kAppStarted);
}

std::shared_ptr<void> Engine::holdStart() {
    auto hold = std::make_shared<bool>(true);
    state->startHolds.push_back(hold);
    return hold;
}

bool Engine::isStartHeld() const noexcept {
    std::erase_if(state->startHolds, [](const std::weak_ptr<void>& hold) { return hold.expired(); });
    return !state->startHolds.empty();
}

// Every start applies the window options of app.json again, so an app that restarts from its package gets the window it asks for, with the mouse, the cursor and the keyboard as a new window has them. The position only applies when the window opens, so a restart leaves the window where the player moved it.
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
    host.setStatusBar(window.statusBar);
    current.statusBarColor = window.statusBarColor;
    host.setMousePassthrough(window.mousePassthrough ? platform::Window::Passthrough::Whole : platform::Window::Passthrough::Off, {});
    host.setMouseLocked(false);
    host.setCursorVisible(true);
    host.setCursor(platform::Window::Cursor::Default);
    host.setKeyboardVisible(false);
}

void Engine::stop() {
    EngineState& current = *state;
    if (!current.started) {
        return;
    }
    current.started = false;
    const PhaseScope scope(*this, Phase::Lifecycle);

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
    current.screenCaptures.clear();
    current.capturingScreen.clear();
    current.graphics->cancelReads();

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

    // Native UI that covered or uncovered the app since the last frame changes its state before the frame decides whether it is halted, and the screen that the app asked for reaches the platform once the cover holds.
    try {
        applyCover();
        current.screens->present();
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
    // Plugins that start during the frame join the next one, and the loops reach the plugins by index, since the registry grows when one starts.
    const std::size_t plugins = current.plugins.size();
    debug::Profiler& profiler = current.profiler;
    profiler.beginFrame(frameSeconds);
    current.lifecycleFailed = false;

    try {
        if (current.recovering) {
            leaveErrorScreen();
        }

        // An app that waited for its holds starts in the first frame after they end, as a lifecycle step.
        if (current.applicationPending && !current.errorScreen && !isStartHeld()) {
            const PhaseScope scope(*this, Phase::Lifecycle);
            startApplication();
        }

        // Window and gamepad changes, asynchronous results and platform replies arrive before the app updates, and their callbacks may fail like any app code.
        publishDeviceChanges();
        if (current.errorScreen) {
            current.errorScreen->update();
        }

        // Decoded assets create their GPU resources within the upload budget of the frame, so a burst of loads never stalls it, and before the event loop runs, so the promises they settle resume their coroutines in this frame. An app in the background creates none.
        if (!hidden) {
            current.assets->finalizePending();
        }
        // The replies of the platform settle their promises before the event loop runs too, so a coroutine that awaits a native call resumes in the frame its reply reaches.
        {
            const debug::ProfileScope scope(profiler, "scripts");
            current.platform->pump();
            current.system->pump();
            current.mediaSession->pump();
            current.accessibility->pump();
            current.tweens.setReducedMotion(current.accessibility->getSettings().reducedMotion);
            current.dialogs->pump();
            current.screens->pump();
            // A screen that ended in the pump uncovers the app before the code that awaited its answer resumes.
            applyCover();
            const std::chrono::nanoseconds budget = getPollBudget(frameSeconds);
            const varn::runtime::EventLoop::PollResult polled = current.runtime->poll(budget);
            current.loopPoll = {.budget = budget, .ran = polled.ran, .pending = polled.pending, .idleMilliseconds = polled.idleMilliseconds};
        }

        if (!hidden) {
            for (std::size_t index = 0; index < plugins; ++index) {
                current.plugins[index].beginFrame(*this, delta);
            }
        }

        if (!current.errorScreen && !halted) {
            const auto step = static_cast<float>(current.clock.getFixedStep());
            while (current.clock.consumeFixedStep()) {
                const debug::ProfileScope scope(profiler, "fixedUpdate");
                current.actions.beginStep(step);
                current.scenes->fixedUpdate(current.clock);
                current.tweens.fixedUpdate(current.clock);
                for (std::size_t index = 0; index < plugins; ++index) {
                    current.plugins[index].fixedUpdate(*this, step);
                }
            }

            const debug::ProfileScope scope(profiler, "update");
            current.timers.update(current.clock);
            current.tweens.update(current.clock);
            for (std::size_t index = 0; index < plugins; ++index) {
                current.plugins[index].update(*this, delta);
            }
            current.scenes->update(current.clock);
        }
        current.audio->update(static_cast<float>(current.clock.getUnscaledDelta()));
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    // A covered app draws one frame once the cover began, which shows it covered and stays on screen, and no more while the cover lasts. An opaque screen of a plugin hides the app, which draws nothing under it.
    if (!hidden && !current.hiddenByScreen && !(current.covered && current.coveredFrameDrawn)) {
        render(plugins);
        current.coveredFrameDrawn = current.covered;
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

    if (std::optional<platform::Fold> fold = getFold(); fold != current.fold) {
        current.fold = fold;
        current.events.emit(LifecycleEvent::kWindowFoldChanged, describeFold(fold));
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

void Engine::render(std::size_t plugins) {
    EngineState& current = *state;
    debug::Profiler& profiler = current.profiler;

    // Rendering records every canvas and submits the whole frame at the end. A frame the app asked to capture renders into an image of the pixel size of the viewport, which then covers the screen.
    current.renderer->beginFrame(current.viewport, current.config.clearColor);
    const bool capturing = !current.screenCaptures.empty();
    if (capturing) {
        std::swap(current.capturingScreen, current.screenCaptures);
        const math::Vec2 size = current.viewport.getPixelRect().getSize();
        const int width = std::max(1, static_cast<int>(std::lround(size.x)));
        const int height = std::max(1, static_cast<int>(std::lround(size.y)));
        if (!current.screenImage.isValid() || current.screenImage.getWidth() != width || current.screenImage.getHeight() != height) {
            current.screenImage = current.graphics->createRenderTarget(width, height);
        }
        current.renderer->beginCapture(current.screenImage, current.config.clearColor);
    }
    try {
        const debug::ProfileScope scope(profiler, "render");
        if (!current.errorScreen) {
            renderScenes(plugins);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    if (current.errorScreen) {
        current.errorScreen->render();
    }
    if (current.statusBarColor) {
        paintStatusBar(*current.statusBarColor);
    }
    try {
        for (std::size_t index = 0; index < plugins; ++index) {
            current.plugins[index].renderOverlay(*this);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    if (capturing) {
        try {
            current.renderer->endCapture();
            current.renderer->beginScreen();
            const math::Rect visible = current.viewport.getVisibleRect();
            current.renderer->draw({.texture = current.screenImage.getTexture(), .position = visible.getMin(), .size = visible.getSize(), .pivot = {0.0F, 0.0F}, .order = {.blend = graphics::BlendMode::Type::Premultiplied}});
        } catch (const std::exception& exception) {
            reportError(exception);
        }
    }
    try {
        const debug::ProfileScope scope(profiler, "submit");
        current.renderer->endFrame(current.host.getFrameTarget());
    } catch (const std::exception& exception) {
        reportError(exception);
    }
    for (graphics::Device::ReadCallback& done : current.capturingScreen) {
        current.graphics->readTarget(current.screenImage, std::move(done));
    }
    current.capturingScreen.clear();
    try {
        current.graphics->processReads(*current.jobs);
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    try {
        for (std::size_t index = 0; index < plugins; ++index) {
            current.plugins[index].endFrame(*this);
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }
}

// The status bar lies over the top of the screen, so its background covers the part of the visible area under it.
void Engine::paintStatusBar(math::Color color) {
    EngineState& current = *state;
    const graphics::Viewport& viewport = current.viewport;
    const float covered = current.host.getStatusBarHeight() - viewport.getPixelRect().y;
    if (covered <= 0.0F) {
        return;
    }
    const math::Rect visible = viewport.getVisibleRect();
    current.renderer->beginScreen();
    current.renderer->drawRect({visible.x, visible.y, visible.width, covered / viewport.getPixelsPerUnit().y}, color);
}

// Each view renders its scenes, with the drawing of the plugins in the current view and the interface of the plugins in every view. During a transition the views before the last one go into their own images, and the last one draws the effect on the screen before its own scenes.
void Engine::renderScenes(std::size_t plugins) {
    EngineState& current = *state;
    graphics2d::Renderer& renderer = *current.renderer;
    SceneManager& scenes = *current.scenes;
    scenes.prepareViews();
    for (const SceneView& view : scenes.getViews()) {
        if (view.target.isValid()) {
            renderer.beginCapture(view.target, current.config.clearColor);
        }
        if (view.effect) {
            scenes.renderTransition(renderer, view);
        }
        scenes.render(view);
        if (view.current) {
            for (std::size_t index = 0; index < plugins; ++index) {
                current.plugins[index].render(*this);
            }
        }
        scenes.renderUi(view);
        for (std::size_t index = 0; index < plugins; ++index) {
            current.plugins[index].renderUi(*this, view);
        }
        if (view.target.isValid()) {
            renderer.endCapture();
        }
    }
}

// The mouse and the fingers reach the app as the device the app asked for, which may turn one event into several, and the scroll speed of the app scales every scroll.
void Engine::handleEvent(const platform::Event& event) {
    platform::Event scaled = event;
    scaled.scroll *= state->input.getScrollSpeed();
    for (const platform::Event& converted : state->pointerEmulation.convert(scaled)) {
        dispatchEvent(converted);
    }
}

void Engine::dispatchEvent(const platform::Event& event) {
    EngineState& current = *state;
    current.input.handleEvent(event, current.viewport);

    // The error screen takes the input of the stopped app, so its actions work even when a plugin fails on the same event.
    if (current.errorScreen) {
        current.errorScreen->handleEvent(event);
    }
    try {
        switch (event.type) {
        case platform::Event::Type::Resized: {
            // The frame on screen no longer fits the window, so a covered app draws it once more.
            current.coveredFrameDrawn = false;
            resized.emit();
            const math::Vec2 size = current.host.getFramebufferSize();
            current.events.emit(LifecycleEvent::kWindowResized, {{"width", JsonNumber::fromFloat(size.x)}, {"height", JsonNumber::fromFloat(size.y)}});
            break;
        }
        case platform::Event::Type::FocusLost:
            // The releases of what is held go to the window that takes the focus, so nothing stays pressed, also in an app that something else made inactive before.
            current.focused = false;
            releaseHeldInput();
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
            setAppState(getForegroundState());
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
        case platform::Event::Type::TextEdited:
            current.appTextField.receive(event.textEdit);
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
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    // The plugins hear the event also when a listener of the engine event failed, so a plugin that keeps data, such as the preferences, still saves it when the app goes to the background.
    try {
        for (std::size_t index = 0, plugins = current.plugins.size(); index < plugins; ++index) {
            current.plugins[index].event(*this, event);
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

    // Leaving the foreground releases held input, including on-screen controls, and suspends audio, and coming back skips the time the app was away. The platform may have dropped the surface of an app in the background, so a covered app draws its frame again when it comes back.
    if (value != AppState::Active) {
        releaseHeldInput();
    }
    if (value == AppState::Background) {
        current.audio->suspend();
    }
    if (previous == AppState::Background) {
        current.audio->resume();
        current.clock.skipNextDelta();
        current.coveredFrameDrawn = false;
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
        // The platform may end an app in the background without warning, so what the app just saved becomes durable now, also when a listener of the event failed.
        try {
            current.events.emit(LifecycleEvent::kAppBackground);
        } catch (...) {
            current.storage->flush();
            throw;
        }
        current.storage->flush();
        break;
    }
}

void Engine::releaseHeldInput() {
    EngineState& current = *state;
    current.input.releaseAll();
    current.pointerEmulation.releaseAll();
    current.gestures.cancel();
    current.virtualInput.clear();
    current.system->stopRumbles();
}

void Engine::refreshForegroundState() {
    if (state->appState == AppState::Background) {
        return;
    }
    setAppState(getForegroundState());
}

// An app in the foreground is active only while its window has the focus, no interruption of the system holds it and no native UI of a plugin covers it, also when it comes back from the background.
Engine::AppState Engine::getForegroundState() const noexcept {
    const EngineState& current = *state;
    return current.focused && !current.interrupted && !current.covered ? AppState::Active : AppState::Inactive;
}

// Covered content must never play under native UI, so a cover halts and mutes the app whatever the lifecycle options say. The covers of the platform, of native libraries and the screen of a plugin all count. The app state changes last, because its listeners may fail.
void Engine::applyCover() {
    EngineState& current = *state;
    const std::optional<platform::ScreenRelay::Screen> screen = platform::ScreenRelay::getShowing();
    current.hiddenByScreen = screen && screen->opaque;
    const bool covered = current.host.isAppCovered() || platform::NativeApi::isAppCovered() || screen.has_value();
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
    if (mute != current.audio->isOutputMuted()) {
        current.audio->setOutputMuted(mute);
    }
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
    current.viewport.update(current.host.getFramebufferSize(), current.config.designSize, current.config.scaling, getSafeAreaInsets(), current.host.getDpiScale());
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

std::optional<platform::Fold> Engine::getFold() const {
    const EngineState& current = *state;
    if (current.foldSimulation) {
        return current.foldSimulation->getFold(current.host.getFramebufferSize(), current.host.getDpiScale());
    }
    return current.host.getFold();
}

void Engine::setFoldSimulation(std::optional<platform::FoldSimulation> value) {
    state->foldSimulation = std::move(value);
}

const std::optional<platform::FoldSimulation>& Engine::getFoldSimulation() const noexcept {
    return state->foldSimulation;
}

// The fold reaches the app in design units, like the safe area, with the posture and the segments of the window on each side of it.
Json Engine::describeFold(const std::optional<platform::Fold>& fold) const {
    const math::Vec2 size = state->host.getFramebufferSize();
    const std::vector<math::Rect> segments = fold ? fold->getSegments(size) : std::vector<math::Rect>{{0.0F, 0.0F, size.x, size.y}};
    Json description{{"posture", platform::Fold::postureName(fold ? fold->getPosture() : platform::Fold::Posture::Flat)}, {"segments", Json::array()}};
    for (const math::Rect& segment : segments) {
        description["segments"].push_back(toDesignJson(segment));
    }
    if (fold) {
        Json area = toDesignJson(fold->bounds);
        area["axis"] = platform::Fold::axisName(fold->axis);
        area["state"] = platform::Fold::stateName(fold->state);
        area["separating"] = fold->separating;
        area["occluding"] = fold->occluding;
        description["fold"] = std::move(area);
    }
    return description;
}

Json Engine::toDesignJson(const math::Rect& pixels) const {
    const graphics::Viewport& viewport = state->viewport;
    const math::Rect area = math::Rect::fromMinMax(viewport.toDesign(pixels.getMin()), viewport.toDesign(pixels.getMax()));
    return {{"x", JsonNumber::fromFloat(area.x)}, {"y", JsonNumber::fromFloat(area.y)}, {"width", JsonNumber::fromFloat(area.width)}, {"height", JsonNumber::fromFloat(area.height)}};
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

void Engine::setStatusBarColor(std::optional<math::Color> value) noexcept {
    state->statusBarColor = value;
}

std::optional<math::Color> Engine::getStatusBarColor() const noexcept {
    return state->statusBarColor;
}

void Engine::setBackLeavesApp(bool value) noexcept {
    state->backLeavesApp = value;
}

bool Engine::canBackLeaveApp() const noexcept {
    return state->backLeavesApp;
}

bool Engine::isBackCaptured() const {
    const EngineState& current = *state;
    if (!current.backLeavesApp || (current.errorScreen && current.recoverable)) {
        return true;
    }
    for (std::size_t index = 0; index < current.plugins.size(); ++index) {
        if (current.plugins[index].isCapturingBack()) {
            return true;
        }
    }
    return false;
}

Engine::PhaseScope::PhaseScope(Engine& owner, Phase value) noexcept : engine(owner), previous(owner.state->phase), exceptions(std::uncaught_exceptions()) {
    owner.state->phase = value;
}

Engine::PhaseScope::~PhaseScope() {
    EngineState& current = *engine.state;
    if (current.phase == Phase::Lifecycle && std::uncaught_exceptions() > exceptions) {
        current.lifecycleFailed = true;
    }
    current.phase = previous;
}

Engine::Phase Engine::getPhase() const noexcept {
    return state->phase;
}

void Engine::reportError(const std::exception& exception) {
    EngineState& current = *state;
    const bool lifecycle = current.phase == Phase::Lifecycle || std::exchange(current.lifecycleFailed, false);
    if (current.errorScreen) {
        return;
    }

    // The player reaches the buttons of the error screen with the mouse, also when the app locked or hid it.
    const auto* scriptError = dynamic_cast<const lua::Error*>(&exception);
    current.errorScreen = std::make_unique<ErrorScreen>(*this, scriptError != nullptr ? *scriptError : lua::Error(exception.what()), !lifecycle);
    current.host.setMouseLocked(false);
    current.host.setCursorVisible(true);
    Log::error("{}", current.errorScreen->getReport());
    const Json report = current.errorScreen->getError().toJson();
    current.host.reportError(report);
    platform::NativeApi::reportError(report);
    errorRaised.emit(current.errorScreen->getError());
    current.events.post(std::string(LifecycleEvent::kAppError), report);
}

void Engine::reportError(const std::string& message) {
    reportError(lua::Error(message));
}

const lua::Error* Engine::getError() const noexcept {
    return state->errorScreen ? &state->errorScreen->getError() : nullptr;
}

bool Engine::isErrorResumable() const noexcept {
    return state->errorScreen && state->errorScreen->isResumable();
}

bool Engine::clearError() noexcept {
    EngineState& current = *state;
    if (!current.errorScreen || !current.errorScreen->isResumable()) {
        return false;
    }
    current.errorScreen.reset();
    current.recovering = false;
    return true;
}

void Engine::setRecoverable(bool value) noexcept {
    state->recoverable = value;
}

bool Engine::isRecoverable() const noexcept {
    return state->recoverable;
}

void Engine::recover() noexcept {
    state->recovering = state->errorScreen != nullptr;
}

// The listeners of appRecovered run before the app updates again, so a scene that failed in every frame is gone before it would fail again.
void Engine::leaveErrorScreen() {
    EngineState& current = *state;
    current.recovering = false;
    const Json error = current.errorScreen->getError().toJson();
    current.errorScreen.reset();
    current.events.emit(LifecycleEvent::kAppRecovered, error);
}

void Engine::requestRestart() noexcept {
    state->restartRequested = true;
}

bool Engine::isRestartRequested() const noexcept {
    return state->restartRequested;
}

void Engine::requestLaunch(const std::filesystem::path& package) {
    std::error_code error;
    if (!std::filesystem::exists(package, error)) {
        throw std::invalid_argument("No app package is at \"" + package.string() + "\". Launch a folder or a zip file that holds \"app.json\".");
    }
    state->launchRequest = package;
}

const std::optional<std::filesystem::path>& Engine::getLaunchRequest() const noexcept {
    return state->launchRequest;
}

bool Engine::isLaunched() const noexcept {
    return state->host.isLaunched();
}

void Engine::quit(std::uint8_t status) {
    state->running = false;
    state->exitStatus = status;
    state->host.requestQuit();
}

bool Engine::isRunning() const noexcept {
    return state->running;
}

std::uint8_t Engine::getExitStatus() const noexcept {
    return state->exitStatus;
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

void Engine::captureScreen(graphics::Device::ReadCallback done) {
    state->screenCaptures.push_back(std::move(done));
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
    return *state->actions.getDefault();
}

input::ActionMaps& Engine::getActionMaps() noexcept {
    return state->actions;
}

input::VirtualInput& Engine::getVirtualInput() noexcept {
    return state->virtualInput;
}

input::PointerEmulation& Engine::getPointerEmulation() noexcept {
    return state->pointerEmulation;
}

input::AppTextField& Engine::getAppTextField() noexcept {
    return state->appTextField;
}

SceneManager& Engine::getScenes() noexcept {
    return *state->scenes;
}

content::ContentManager& Engine::getContent() noexcept {
    return *state->content;
}

assets::Manager& Engine::getAssets() noexcept {
    return *state->assets;
}

platform::Bridge& Engine::getPlatform() noexcept {
    return *state->platform;
}

platform::System& Engine::getSystem() noexcept {
    return *state->system;
}

platform::MediaSession& Engine::getMediaSession() noexcept {
    return *state->mediaSession;
}

platform::Accessibility& Engine::getAccessibility() noexcept {
    return *state->accessibility;
}

platform::Dialogs& Engine::getDialogs() noexcept {
    return *state->dialogs;
}

platform::Screens& Engine::getScreens() noexcept {
    return *state->screens;
}

plugins::PluginRegistry& Engine::getPlugins() noexcept {
    return state->plugins;
}

const std::shared_ptr<text::Font>& Engine::getDefaultFont() noexcept {
    return state->defaultFont;
}

const Engine::EventLoopPoll& Engine::getEventLoopPoll() const noexcept {
    return state->loopPoll;
}

std::chrono::nanoseconds Engine::getPollBudget(double frameSeconds) noexcept {
    const std::chrono::duration<double> share(frameSeconds * kPollBudgetShare);
    return std::clamp(std::chrono::duration_cast<std::chrono::nanoseconds>(share), kMinimumPollBudget, kMaximumPollBudget);
}

varn::runtime::Runtime& Engine::getScriptRuntime() noexcept {
    return *state->runtime;
}

lua_State* Engine::getLuaState() noexcept {
    return state->runtime->luaState();
}

} // namespace haylen::core
