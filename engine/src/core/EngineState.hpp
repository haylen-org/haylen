#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/ErrorScreen.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/TweenManager.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/AppTextField.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/PointerEmulation.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/platform/Dialogs.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/Screens.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/plugins/PluginRegistry.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/Host.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::core {

// Everything one engine owns, kept out of the public header so apps do not compile every subsystem.
struct EngineState {
    EngineState(platform::Host& owner, std::shared_ptr<io::Package> source, AppConfig settings, std::unique_ptr<Application> app) : host(owner), package(std::move(source)), config(std::move(settings)), application(std::move(app)), lifecycle(config.lifecycle), appTextField(owner.getTextInput()), clock(1.0 / config.fixedRate, config.maxFrameTime), events(frameQueue) {}

    platform::Host& host;
    std::shared_ptr<io::Package> package;
    AppConfig config;
    std::unique_ptr<Application> application;
    AppConfig::Lifecycle lifecycle;
    input::AppTextField appTextField;
    std::unique_ptr<graphics::Device> graphics;
    std::unique_ptr<audio::Mixer> audio;
    std::unique_ptr<storage::UserStorage> storage;
    graphics::Viewport viewport;
    input::Input input;
    input::GestureRecognizer gestures;
    input::ActionMap actions;
    input::VirtualInput virtualInput;
    input::PointerEmulation pointerEmulation;
    FrameClock clock;
    FrameQueue frameQueue;
    EventBus events;
    TimerScheduler timers;
    TweenManager tweens;
    debug::Profiler profiler;
    std::unique_ptr<platform::Bridge> platform;
    std::unique_ptr<platform::System> system;
    std::unique_ptr<platform::Dialogs> dialogs;
    std::unique_ptr<platform::Screens> screens;
    std::unique_ptr<graphics2d::Renderer> renderer;
    std::unique_ptr<assets::Manager> assets;
    std::unique_ptr<SceneManager> scenes;
    plugins::PluginRegistry plugins;
    // The plugins whose start completed, in start order, which are the ones stop reaches.
    std::vector<plugins::Plugin*> startedPlugins;
    std::shared_ptr<text::Font> defaultFont;
    std::unique_ptr<varn::runtime::Runtime> runtime;
    std::unique_ptr<JobSystem> jobs;
    std::array<input::GamepadState, input::Input::kMaxGamepads> gamepads{};

    // The window, the keyboard, the network and the gamepads as the app last heard of them, so every change is published once. A connected gamepad keeps its name here so its disconnection can report it.
    bool fullscreen = false;
    math::Vec2 windowPosition;
    platform::Orientation orientation = platform::Orientation::Landscape;
    math::Rect safeRect;
    math::Insets reservedInsets;
    std::optional<platform::SafeAreaSimulation> safeAreaSimulation;
    bool backLeavesApp = true;
    math::Rect keyboardFrame;
    Engine::NetworkState network = Engine::NetworkState::Unknown;
    std::array<std::optional<std::string>, input::Input::kMaxGamepads> gamepadNames;

    std::unique_ptr<ErrorScreen> errorScreen;
    bool recoverable = false;
    bool recovering = false;
    Engine::Phase phase = Engine::Phase::Frame;

    // Whether an error left a lifecycle scope since the frame began, which makes the error the engine reports next a lifecycle error.
    bool lifecycleFailed = false;
    Engine::AppState appState = Engine::AppState::Active;

    // An app in the foreground is active only while its window has the focus, no interruption of the system, such as a phone call, holds it and no native UI of a plugin covers it.
    bool focused = true;
    bool interrupted = false;
    bool covered = false;

    // Whether the screen of a plugin that covers the app is opaque, so the app draws nothing under it.
    bool hiddenByScreen = false;

    // Whether the covered app drew the frame that stays on screen under the cover, which a halted app would only draw again the same, until the window changes size or the app comes back from the background.
    bool coveredFrameDrawn = false;
    bool started = false;
    bool running = true;
    bool restartRequested = false;

    // Whether the app state muted the master bus, through muteOnFocusLoss or a cover, and whether it was muted before, so the app coming back restores the choice of the player.
    bool stateMuted = false;
    bool mutedBeforeState = false;
};

} // namespace haylen::core
