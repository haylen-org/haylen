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
#include <string_view>
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
#include "haylen/platform/native/NativeApi.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "platform/LinkRelay.hpp"
#include "platform/ScreenRelay.hpp"
#include "plugins/BuiltInPlugins.hpp"

namespace haylen::core {

Engine::Claim::Claim() {
    if (getClaimed().exchange(true)) {
        throw std::logic_error("Only one engine can run at a time. Destroy the engine that runs before creating another.");
    }
}

Engine::Claim::~Claim() {
    getClaimed().store(false);
}

std::atomic<bool>& Engine::Claim::getClaimed() noexcept {
    static std::atomic<bool>& claimed = *new std::atomic<bool>(false);
    return claimed;
}

Engine::Engine(platform::Host& host, std::shared_ptr<io::Package> package, AppConfig config, std::unique_ptr<Application> application) {
    if (!package || !application) {
        throw std::invalid_argument("The engine needs a package and an application.");
    }

    state = std::make_unique<EngineState>(host, std::move(package), std::move(config), std::move(application));
    EngineState& current = *state;

    // The job system comes first because asset loading, the services and the runtimes of scripts all run on its scheduler. A job runs once, so the app restarts after its failure instead of resuming without what the job would have done.
    // clang-format off
    current.jobs = std::make_unique<JobSystem>([this](const std::string& message) {
        const PhaseScope scope(*this, Phase::Lifecycle);
        reportError(message);
    });
    // clang-format on
    current.graphics = std::make_unique<graphics::Device>(host.createGpuDevice());
    audio::Mixer::Setup audioSetup = host.getAudioSetup();
    audioSetup.session = current.config.audioSession;
    audioSetup.latency = current.config.audioOutput.latency;
    current.audio = std::make_unique<audio::Mixer>(audioSetup, *current.jobs);
    host.setBackgroundAudio(current.backgroundAudio);

    if (current.config.audioOutput.limiter) {
        current.audio->addBusEffect("master", std::make_shared<audio::Limiter>(audio::Limiter::Settings{}));
    }

    // An app without an identifier, such as the screen of an app that did not load, keeps no files, so it never reaches the storage of another app.
    if (current.config.identifier.empty()) {
        current.storage = std::make_unique<storage::UserStorage>(std::filesystem::path{}, std::function<void()>{}, std::string(kNoIdentityRefusal));
    } else {
        // clang-format off
        current.storage = std::make_unique<storage::UserStorage>(host.getUserDataDirectory(current.config.identifier), [&host] { host.persistUserData(); }, host.getUserDataRefusal());
        // clang-format on
    }

    // Handlers that native libraries register through the C interface answer before the handlers of the platform.
    // clang-format off
    current.platform = std::make_unique<platform::Bridge>(
        [&host](std::uint64_t id, std::string_view method, std::string_view params, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
            if (!platform::NativeApi::dispatch(id, method, params, buffers)) {
                host.dispatchPlatformCall(id, method, params, buffers, views);
            }
        },
        [&host](std::uint64_t id, std::string_view method) {
            if (!platform::NativeApi::cancel(id, method)) {
                host.cancelPlatformCall(id);
            }
        });
    // clang-format on

    current.system = std::make_unique<platform::System>(host, current.events, current.graphics->getAdapterName());
    current.gamepadMotion = std::make_unique<input::GamepadMotion>(host);
    current.displays = std::make_unique<platform::Displays>(host, current.events);
    current.outputs = std::make_unique<platform::Outputs>(host, *current.displays, *current.graphics, current.events);
    current.accessibility = std::make_unique<platform::Accessibility>(host, current.events);
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
    current.viewport.update(host.getFramebufferSize(), getOrientedDesignSize(host.getFramebufferSize()), current.config.scaling, getSafeAreaInsets(), host.getDpiScale());
    current.fullscreen = host.isFullscreen();
    current.windowPosition = host.getFrame().getPosition();
    current.orientation = host.getOrientation();
    current.safeRect = current.viewport.getSafeRect();
    current.fold = getFold();

    plugins::BuiltInPlugins::registerAll(current.plugins, host);
    current.application->addPlugins(*this);
}

Engine::EntryScope::EntryScope(const Engine& owner, std::string_view stage) noexcept : observer(owner.state->entryObserver) {
    if (observer != nullptr) {
        observer->enter(stage);
    }
}

Engine::EntryScope::~EntryScope() {
    if (observer != nullptr) {
        observer->leave();
    }
}

void Engine::setEntryObserver(EntryObserver* value) noexcept {
    state->entryObserver = value;
}

// The end of the engine counts as the stop of the app, so the code of the app that runs while the plugins let go of it, such as its finalizers, is timed like the rest of the stop.
Engine::~Engine() {
    const EntryScope entry(*this, "stop");
    stop();

    // Everything that holds values of the scripts of the app lets go before the plugin that runs them closes their runtime, including scenes that stop hooks requested and work still queued on the pools, and the worker pools stop before the plugins, assets and GPU resources that their work still references. The job system drops what is posted from then on, so what the closing runtime and the services after it post is dropped at once, and it outlives every service that holds it.
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
    current.accessibility.reset();
    current.system->stopRumbles();
    current.system.reset();
    current.gamepadMotion->clear();
    current.outputs.reset();
    current.displays.reset();
    current.jobs->stop();

    // The plugins go in reverse order, so a plugin that runs scripts closes their runtime before the plugins it builds on go, and the motion of gamepads, audio and assets outlive it, since its owners still end their requests while it closes.
    current.plugins.clear();
    current.gamepadMotion.reset();
    current.audio.reset();
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
    state->events.emit(LifecycleEvent::kPluginStarted, {{"name", std::string(plugin.getName())}});
}

void Engine::start() {
    const EntryScope entry(*this, "start");
    EngineState& current = *state;
    if (current.started) {
        return;
    }

    current.started = true;
    current.starting = true;

    applyWindowOptions();

    // A plugin or an application that fails to start shows the error screen instead of taking the process down.
    const PhaseScope scope(*this, Phase::Lifecycle);

    try {
        for (plugins::Plugin* plugin : current.plugins.getAll()) {
            plugin->start(*this);
            current.startedPlugins.push_back(plugin);
            current.events.emit(LifecycleEvent::kPluginStarted, {{"name", std::string(plugin->getName())}});
        }

        if (isStartHeld()) {
            current.applicationPending = true;
            current.starting = false;
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
    current.starting = true;
    current.application->start(*this);
    current.events.emit(LifecycleEvent::kAppStarted);
    current.starting = false;
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

// Every start applies the window options of haylen.json again, so an app that restarts from its package gets the window it asks for, with the mouse, the cursor and the keyboard as a new window has them. The position only applies when the window opens, so a restart leaves the window where the player moved it.
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
    host.setSystemGestures(window.systemGestures);
    host.setExternalDisplays(window.externalDisplays);

    // clang-format off
    host.setKeepScreenOn(window.keepScreenOn, [](const platform::SystemAnswer& answer) {
        if (answer.state != platform::SystemAnswer::State::Granted) {
            Log::warning("The screen follows the settings of the system instead of the \"window.keepScreenOn\" of \"haylen.json\". {}", answer.message);
        }
    });
    // clang-format on

    host.setFrameRate(window.frameRate);
    host.setMaxPixelDensity(window.maxPixelDensity);
    current.statusBarColor = window.statusBarColor;
    host.setMousePassthrough(window.mousePassthrough ? platform::Window::Passthrough::Whole : platform::Window::Passthrough::Off, {});

    // Every platform grants the mode `Normal`, so the mouse goes free without an answer.
    host.setMouseMode(platform::Window::MouseMode::Normal, {});
    host.setCursor(platform::Window::Cursor::Default);
    host.setKeyboardVisible(false);
}

void Engine::stop() {
    const EntryScope entry(*this, "stop");
    EngineState& current = *state;
    if (!current.started) {
        return;
    }

    current.started = false;
    const PhaseScope scope(*this, Phase::Lifecycle);

    // Stopping runs app code that may fail like any other, and every part still stops so nothing holds values of the scripts of the app once their runtime closes.
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
    current.graphics->cancelCapture();

    const std::vector<plugins::Plugin*> started = std::exchange(current.startedPlugins, {});
    for (auto plugin = started.rbegin(); plugin != started.rend(); ++plugin) {
        attempt([this, plugin] { (*plugin)->stop(*this); });
        attempt([&current, plugin] { current.events.emit(LifecycleEvent::kPluginStopped, {{"name", std::string((*plugin)->getName())}}); });
    }
}

void Engine::frame(double frameSeconds) {
    const EntryScope entry(*this, "frame");
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

    // A halted app lets no time pass, and an app in the background draws nothing, so it does no GPU work. A covered app draws one frame once the cover began, which shows it covered and stays on screen, and no more while the cover lasts, and an opaque screen of a plugin hides the app, which draws nothing under it. A window without area, such as a canvas that its page hides, shows nothing, so the app draws nothing and lays nothing out until it has an area again.
    const bool halted = isHalted();
    const bool hidden = current.appState == AppState::Background;
    const bool taking = current.host.isTakingFrames();
    current.drawing = !hidden && taking && !current.hiddenByScreen && !(current.covered && current.coveredFrameDrawn) && hasWindowArea();
    remapViewport();
    current.gamepadMotion->update(!hidden);
    current.host.pollGamepads(current.gamepads);
    current.input.updateGamepads(current.gamepads);
    current.actions.update(current.input, current.virtualInput, halted || current.scenes->isInputBlocked());

    current.clock.advance(halted ? 0.0 : frameSeconds);
    const auto delta = static_cast<float>(current.clock.getDelta());
    current.input.updateTouchDurations(static_cast<float>(current.clock.getUnscaledDelta()));
    current.gestures.update(current.input, static_cast<float>(current.clock.getUnscaledDelta()));

    // Only the plugins that started take part in the frame. Plugins that start during the frame join the next one, and the loops reach them by index, since the list grows when one starts.
    const std::size_t plugins = current.startedPlugins.size();
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
        if (!hidden && current.assets->finalizePending()) {
            requestRedraw();
        }

        runScripts(frameSeconds);

        // A frame that an app that redraws on change skips draws nothing and updates nothing, so the picture of the frame before stays on the screen, and it only lets the time of the timers pass and keeps its time out of the fixed steps, so the frame after it neither jumps nor catches up. A change that a frame updated while the window system took no frame draws once it takes frames again.
        if (taking && std::exchange(current.drawMissed, false)) {
            requestRedraw();
        }

        const bool updating = takeRedraw(halted);
        current.drawMissed = current.drawMissed || (updating && !taking);
        current.drawing = current.drawing && updating;

        if (!hidden) {
            for (std::size_t index = 0; index < plugins; ++index) {
                current.startedPlugins[index]->beginFrame(*this, delta);
            }
        }

        if (!current.errorScreen && !halted && updating) {
            updateApp(plugins, delta);
        } else if (!current.errorScreen && !halted) {
            current.clock.skipFixedSteps();
            current.timers.advance(current.clock);
        }

        current.audio->update(static_cast<float>(current.clock.getUnscaledDelta()));
        followBackgroundPlayback();
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    if (current.drawing) {
        render(plugins);
        current.coveredFrameDrawn = current.covered;
        current.frameComplete = checkFrameComplete();
    }

    // Deferred signal slots and queued events run last, after everything the frame did, so what they change shows in the next frame.
    try {
        if (current.frameQueue.size() > 0) {
            requestRedraw();
        }

        current.frameQueue.flush();
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    current.graphics->collectGarbage();
    current.graphics->followSpareMemory(frameSeconds);
    current.input.endFrame();
    profiler.endFrame();
}

// The replies of the platform settle their promises before the event loop runs, so a coroutine that awaits a native call resumes in the frame its reply reaches, and whatever arrived asks an app that redraws on change for this frame.
void Engine::runScripts(double frameSeconds) {
    EngineState& current = *state;
    const debug::ProfileScope scope(current.profiler, "scripts");
    const bool replied = current.platform->pump();
    const bool answered = current.system->pump();
    publishLinks();
    const bool accessed = current.accessibility->pump();
    current.tweens.setReducedMotion(current.accessibility->getSettings().reducedMotion);
    const bool ended = current.screens->pump();
    if (replied || answered || accessed || ended) {
        requestRedraw();
    }

    // A screen that ended in the pump uncovers the app before the code that awaited its answer resumes.
    applyCover();

    const std::chrono::nanoseconds budget = getPollBudget(frameSeconds);
    const JobSystem::Poll polled = current.jobs->poll(budget);
    current.loopPoll = {.budget = budget, .ran = polled.ran, .pending = polled.pending, .idleMilliseconds = polled.idleMilliseconds};
}

// An app that redraws always needs every frame. The frame that a change needs starts the settle of the frames after it, and a frame without a change still draws while the settle lasts.
bool Engine::takeRedraw(bool halted) {
    EngineState& current = *state;
    if (current.redraw == platform::Window::Redraw::Always) {
        return true;
    }

    const bool needed = isRedrawNeeded(halted);
    current.redrawRequested = false;
    current.heardEvents = current.events.getHeardCount();
    if (needed) {
        current.settleFrames = kSettleFrames;
        return true;
    }

    if (current.settleFrames > 0) {
        --current.settleFrames;
        return true;
    }

    return false;
}

// The checks run from the cheapest, and the plugins answer last.
bool Engine::isRedrawNeeded(bool halted) const {
    const EngineState& current = *state;
    if (current.redrawRequested || current.loopPoll.ran || current.events.getHeardCount() != current.heardEvents) {
        return true;
    }

    if (!current.frameComplete || current.errorScreen || current.applicationPending || current.input.isActive() || current.scenes->isTransitioning()) {
        return true;
    }

    if (!current.screenCaptures.empty() || current.graphics->isCapturing() || current.graphics->getPendingReadCount() > 0) {
        return true;
    }

    if (!halted && (current.timers.isDue(current.clock) || current.tweens.isAnimating(current.clock))) {
        return true;
    }

    for (std::size_t index = 0; index < current.startedPlugins.size(); ++index) {
        if (current.startedPlugins[index]->isAnimating()) {
            return true;
        }
    }

    return false;
}

// The fixed steps the clock owes run first, then the frame update of the timers, the tweens, the plugins and the scenes.
void Engine::updateApp(std::size_t plugins, float delta) {
    EngineState& current = *state;
    debug::Profiler& profiler = current.profiler;
    const auto step = static_cast<float>(current.clock.getFixedStep());
    while (current.clock.consumeFixedStep()) {
        const debug::ProfileScope scope(profiler, "fixedUpdate");
        current.actions.beginStep(step);
        current.scenes->fixedUpdate(current.clock);
        current.tweens.fixedUpdate(current.clock);

        for (std::size_t index = 0; index < plugins; ++index) {
            current.startedPlugins[index]->fixedUpdate(*this, step);
        }
    }

    const debug::ProfileScope scope(profiler, "update");
    current.timers.update(current.clock);
    current.tweens.update(current.clock);

    for (std::size_t index = 0; index < plugins; ++index) {
        current.startedPlugins[index]->update(*this, delta);
    }

    current.scenes->update(current.clock);
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

    if (current.safeAreaSimulation) {
        current.safeAreaSimulation->getCutouts(current.host.getFramebufferSize(), current.readCutouts);
    } else {
        current.host.getCutouts(current.readCutouts);
    }

    if (current.readCutouts != current.cutouts) {
        std::swap(current.readCutouts, current.cutouts);
        Json cutouts = Json::array();
        for (const math::Rect& cutout : current.cutouts) {
            cutouts.push_back(toDesignJson(cutout));
        }

        current.events.emit(LifecycleEvent::kWindowCutoutsChanged, {{"cutouts", std::move(cutouts)}});
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
            current.startedPlugins[index]->renderOverlay(*this);
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

    for (const std::shared_ptr<platform::Output>& output : current.outputs->getAll()) {
        if (output->getSurface() != 0) {
            current.renderer->present(output->getTarget(), output->getSurface(), output->getSurfaceSize(), output->getClearColor());
        }
    }

    try {
        const debug::ProfileScope scope(profiler, "submit");
        current.renderer->endFrame(current.host.isFrameTransparent());
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    try {
        current.graphics->processCapture();
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
            current.startedPlugins[index]->endFrame(*this);
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
                current.startedPlugins[index]->render(*this);
            }
        }

        scenes.renderUi(view);

        for (std::size_t index = 0; index < plugins; ++index) {
            current.startedPlugins[index]->renderUi(*this, view);
        }

        if (view.target.isValid()) {
            renderer.endCapture();
        }
    }
}

// The mouse and the fingers reach the app as the device the app asked for, which may turn one event into several, and the scroll speed of the app scales every scroll.
void Engine::handleEvent(const platform::Event& event) {
    const EntryScope entry(*this, "event");
    platform::Event scaled = event;
    scaled.scroll *= state->input.getScrollSpeed();
    for (const platform::Event& converted : state->pointerEmulation.convert(scaled)) {
        dispatchEvent(converted);
    }
}

// Every event of the platform may change what the app shows, so it asks an app that redraws on change for the next frame.
void Engine::dispatchEvent(const platform::Event& event) {
    EngineState& current = *state;
    requestRedraw();

    // The reports and the pointers of outputs on other displays belong to the outputs, apart from the input of the window.
    if (event.output != 0) {
        try {
            if (event.type == platform::Event::Type::OutputChanged) {
                current.outputs->refresh(event.output);
            } else {
                current.outputs->receiveInput(event);
            }
        } catch (const std::exception& exception) {
            reportError(exception);
        }

        return;
    }

    current.input.handleEvent(event, current.viewport);

    // The error screen takes the input of the stopped app, so its actions work even when a plugin fails on the same event.
    if (current.errorScreen) {
        current.errorScreen->handleEvent(event);
    }

    try {
        switch (event.type) {
        case platform::Event::Type::Resized: {
            // The frame on screen no longer fits the window, so a covered app draws it once more. A window without area keeps the size that the app knows, which hears the next size with an area.
            current.coveredFrameDrawn = false;
            if (hasWindowArea()) {
                resized.emit();
                const math::Vec2 size = current.host.getFramebufferSize();
                current.events.emit(LifecycleEvent::kWindowResized, {{"width", JsonNumber::fromFloat(size.x)}, {"height", JsonNumber::fromFloat(size.y)}});
            }

            current.displays->refreshWindowDisplay();
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

            current.displays->refreshWindowDisplay();
            break;
        }
        case platform::Event::Type::DisplaysChanged:
            current.displays->refresh();
            current.outputs->followDisplays();
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
            // The GPU device gives back what it holds for later once the assets and the listeners of the app released what they could.
            current.assets->releaseUnused();
            lowMemory.emit(event.memoryLevel);
            current.events.emit(LifecycleEvent::kAppLowMemory, {{"level", platform::Event::memoryLevelName(event.memoryLevel)}});
            current.graphics->trim();
            break;
        default:
            break;
        }
    } catch (const std::exception& exception) {
        reportError(exception);
    }

    // The plugins hear the event also when a listener of the engine event failed, so a plugin that keeps data, such as the preferences, still saves it when the app goes to the background.
    try {
        for (std::size_t index = 0, plugins = current.startedPlugins.size(); index < plugins; ++index) {
            current.startedPlugins[index]->event(*this, event);
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

    // Leaving the foreground releases held input, including on-screen controls, and suspends audio unless the app plays in the background, and coming back skips the time the app was away. The platform may have dropped the surface of an app in the background, so a covered app draws its frame again when it comes back.
    if (value != AppState::Active) {
        releaseHeldInput();
    }

    if (value == AppState::Background && !current.backgroundAudio) {
        current.audio->suspend();
    }

    if (previous == AppState::Background) {
        current.backgroundSilent = false;
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
        // The platform may end an app in the background without warning, so what the app just saved becomes durable now, also when a listener of the event failed. A storage that the platform keeps from the app holds nothing to save.
        try {
            current.events.emit(LifecycleEvent::kAppBackground);
        } catch (...) {
            flushUserData();
            throw;
        }

        flushUserData();
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

// The links that opened the app wait until it started, so the listeners of its main script hear the link that launched it. The schemes of the links are those of the app that the process installed, so a link ends a launched app, and the app that launched it hears the link once it starts again.
void Engine::publishLinks() {
    EngineState& current = *state;
    if (current.applicationPending) {
        return;
    }

    if (isLaunched()) {
        if (platform::LinkRelay::hasLinks()) {
            quit();
        }

        return;
    }

    for (std::string& url : platform::LinkRelay::take()) {
        current.events.emit(LifecycleEvent::kLinkOpened, {{"url", std::move(url)}});
    }
}

// An app that plays in the background keeps its output there only while a voice plays, so a platform that runs such apps, as iOS does, suspends the app once its sound ended or paused, and a voice that starts again, such as from the controls of the lock screen, brings the output back.
void Engine::followBackgroundPlayback() {
    EngineState& current = *state;
    if (current.appState != AppState::Background || !current.backgroundAudio) {
        return;
    }

    const bool silent = !current.audio->isPlaying();
    if (silent == current.backgroundSilent) {
        return;
    }

    current.backgroundSilent = silent;
    if (silent) {
        current.audio->suspend();
        return;
    }

    current.audio->resume();
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

// The output of an app in the background follows the change at once: it stops when the sound may no longer play there, and plays again in the next frame when a voice plays.
void Engine::setBackgroundAudio(bool value) {
    EngineState& current = *state;
    if (value && !current.backgroundAudioDeclared) {
        throw std::invalid_argument("The app plays in the background only when it declares that to the system, with the mode \"audio\" of \"UIBackgroundModes\" in its \"Info.plist\" on Apple platforms and a service of the type \"mediaPlayback\" in its manifest on Android, which a plugin that plays in the background brings.");
    }

    if (value == current.backgroundAudio) {
        return;
    }

    current.backgroundAudio = value;
    current.host.setBackgroundAudio(value);

    if (current.appState != AppState::Background) {
        return;
    }

    current.backgroundSilent = value;
    if (!value) {
        current.audio->suspend();
    }
}

bool Engine::isBackgroundAudio() const noexcept {
    return state->backgroundAudio;
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

// Input positions live in design units, so they move with the viewport whenever it maps the screen differently, such as after a resize or a rotation. A window without area keeps the last mapping, which the app lays out for, until it has an area again.
void Engine::remapViewport() {
    EngineState& current = *state;
    if (!hasWindowArea()) {
        return;
    }

    const graphics::Viewport previous = current.viewport;
    current.reservedInsets = current.host.getReservedInsets();
    const math::Vec2 framebuffer = current.host.getFramebufferSize();
    current.viewport.update(framebuffer, getOrientedDesignSize(framebuffer), current.config.scaling, getSafeAreaInsets(), current.host.getDpiScale());

    if (current.viewport.getPixelRect() != previous.getPixelRect() || current.viewport.getVisibleRect() != previous.getVisibleRect()) {
        current.input.followViewport(previous, current.viewport);
    }

    if (current.viewport.getFramebufferSize() != previous.getFramebufferSize() || current.viewport.getDesignSize() != previous.getDesignSize() || current.viewport.getPixelRect() != previous.getPixelRect() || current.viewport.getSafeRect() != previous.getSafeRect()) {
        requestRedraw();
    }
}

void Engine::flushUserData() {
    storage::UserStorage& files = *state->storage;
    if (files.isAvailable()) {
        files.flush();
    }
}

// A window without area, such as a canvas that its page hides, has a framebuffer of no pixels.
bool Engine::hasWindowArea() const noexcept {
    const math::Vec2 framebuffer = state->host.getFramebufferSize();
    return framebuffer.x >= 1.0F && framebuffer.y >= 1.0F;
}

// An app that turns with the device lays its design out along the screen, so a design taller than wide turns wide on a screen wider than tall, and the app keeps the size of its design units and lays out again as a native app does.
math::Vec2 Engine::getOrientedDesignSize(math::Vec2 framebuffer) const noexcept {
    const math::Vec2 design = state->config.designSize;
    if (state->config.orientation != platform::Orientation::Any) {
        return design;
    }

    const bool turned = (framebuffer.x > framebuffer.y && design.x < design.y) || (framebuffer.x < framebuffer.y && design.x > design.y);
    return turned ? math::Vec2{design.y, design.x} : design;
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

std::span<const math::Rect> Engine::getCutouts() const noexcept {
    return state->cutouts;
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
    std::vector<platform::AppPlugin> all = platform::AppPlugin::readAll(*current.package, current.config.plugins);
    for (platform::AppPlugin& plugin : all) {
        plugin.native = std::ranges::binary_search(loaded, plugin.id);
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

    for (std::size_t index = 0; index < current.startedPlugins.size(); ++index) {
        if (current.startedPlugins[index]->isCapturingBack()) {
            return true;
        }
    }

    return false;
}

void Engine::setRedraw(platform::Window::Redraw value) noexcept {
    state->redraw = value;
    requestRedraw();
}

platform::Window::Redraw Engine::getRedraw() const noexcept {
    return state->redraw;
}

void Engine::requestRedraw() noexcept {
    state->redrawRequested = true;
}

bool Engine::isFrameComplete() const noexcept {
    return state->frameComplete;
}

bool Engine::isDrawing() const noexcept {
    return state->drawing;
}

// The first scene that loads without a loading view leaves the screen to the clear color until it enters.
bool Engine::checkFrameComplete() const {
    const EngineState& current = *state;
    if (current.errorScreen || current.applicationPending) {
        return true;
    }

    const SceneManager& scenes = *current.scenes;
    if (scenes.empty() && scenes.isTransitioning() && !scenes.isLoadingViewShown()) {
        return false;
    }

    for (std::size_t index = 0; index < current.startedPlugins.size(); ++index) {
        if (!current.startedPlugins[index]->isFrameComplete()) {
            return false;
        }
    }

    return true;
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

    const auto* scriptError = dynamic_cast<const core::AppError*>(&exception);
    showError(scriptError != nullptr ? *scriptError : core::AppError(exception.what()), !lifecycle);
}

void Engine::cancel(const core::AppError& error) {
    EngineState& current = *state;
    current.cancelled = true;
    current.recovering = false;
    current.errorScreen.reset();
    showError(error, false);
}

bool Engine::isCancelled() const noexcept {
    return state->cancelled;
}

// The player reaches the buttons of the error screen with the mouse, also when the app hid, locked or confined it.
void Engine::showError(const core::AppError& error, bool resumable) {
    EngineState& current = *state;
    current.errorScreen = std::make_unique<ErrorScreen>(*this, error, resumable);
    current.host.setMouseMode(platform::Window::MouseMode::Normal, {});

    Log::error("{}", current.errorScreen->getReport());
    current.errorPhase = current.starting ? std::string_view("startup") : kPhaseNames[static_cast<std::size_t>(current.phase)];

    // An error ends the start of the app, so the errors after a recovery name the phase they happen in.
    current.starting = false;

    const Json report = getErrorRecord();
    current.host.reportError(report);
    platform::NativeApi::reportError(report);
    errorRaised.emit(current.errorScreen->getError());
    current.events.post(std::string(LifecycleEvent::kAppError), report);
}

void Engine::reportError(const std::string& message) {
    reportError(core::AppError(message));
}

const core::AppError* Engine::getError() const noexcept {
    return state->errorScreen ? &state->errorScreen->getError() : nullptr;
}

Json Engine::getErrorRecord() const {
    if (!state->errorScreen) {
        return nullptr;
    }
    Json record = state->errorScreen->getError().toJson();
    record["phase"] = state->errorPhase;
    return record;
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
    return state->recoverable && !state->cancelled;
}

void Engine::recover() noexcept {
    state->recovering = state->errorScreen != nullptr && !state->cancelled;
}

// The listeners of appRecovered run before the app updates again, so a scene that failed in every frame is gone before it would fail again.
void Engine::leaveErrorScreen() {
    EngineState& current = *state;
    current.recovering = false;
    const Json error = getErrorRecord();
    current.errorScreen.reset();
    current.events.emit(LifecycleEvent::kAppRecovered, error);
}

void Engine::requestRestart() noexcept {
    state->restartRequested = true;
}

bool Engine::isRestartRequested() const noexcept {
    return state->restartRequested;
}

void Engine::requestLaunch(Launch launch) {
    std::error_code error;
    if (!std::filesystem::exists(launch.package, error)) {
        throw std::invalid_argument("No app package is at \"" + launch.package.string() + "\". Launch a folder or a zip file that holds \"haylen.json\".");
    }

    if (launch.address && launch.address->find("://") == std::string::npos) {
        throw std::invalid_argument("The address \"" + *launch.address + "\" of the launch holds no \"://\". Name the address that the package came from, such as \"https://example.com/apps/garden.zip\".");
    }

    const std::string_view server = launch.developmentServer;
    if (!server.empty() && !server.starts_with("ws://") && !server.starts_with("wss://")) {
        throw std::invalid_argument("The development server \"" + launch.developmentServer + "\" is not an address that starts with \"ws://\" or \"wss://\".");
    }

    state->launchRequest = std::move(launch);
}

const std::optional<Engine::Launch>& Engine::getLaunchRequest() const noexcept {
    return state->launchRequest;
}

bool Engine::isLaunched() const noexcept {
    return state->host.isLaunched();
}

bool Engine::hasNativeAccess() const noexcept {
    return state->host.hasNativeAccess();
}

platform::DevelopmentSession* Engine::getDevelopmentSession() const noexcept {
    return state->host.getDevelopmentSession();
}

std::span<const std::string> Engine::getLaunchArguments() const noexcept {
    return state->host.getLaunchArguments();
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

input::GamepadMotion& Engine::getGamepadMotion() noexcept {
    return *state->gamepadMotion;
}

platform::System& Engine::getSystem() noexcept {
    return *state->system;
}

platform::Displays& Engine::getDisplays() noexcept {
    return *state->displays;
}

platform::Outputs& Engine::getOutputs() noexcept {
    return *state->outputs;
}

platform::Accessibility& Engine::getAccessibility() noexcept {
    return *state->accessibility;
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

} // namespace haylen::core
