#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

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
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/plugins/PluginRegistry.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/Host.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::core {

// Everything one engine owns, kept out of the public header so apps do not compile every subsystem.
struct EngineState {
    EngineState(platform::Host& owner, std::shared_ptr<io::Package> source, AppConfig settings, std::unique_ptr<Application> app) : host(owner), package(std::move(source)), config(std::move(settings)), application(std::move(app)), lifecycle(config.lifecycle), clock(1.0 / config.fixedRate, config.maxFrameTime), events(frameQueue) {}

    platform::Host& host;
    std::shared_ptr<io::Package> package;
    AppConfig config;
    std::unique_ptr<Application> application;
    AppConfig::Lifecycle lifecycle;
    std::unique_ptr<graphics::Device> graphics;
    std::unique_ptr<audio::Mixer> audio;
    std::unique_ptr<storage::UserStorage> storage;
    graphics::Viewport viewport;
    input::Input input;
    input::GestureRecognizer gestures;
    input::ActionMap actions;
    input::VirtualInput virtualInput;

    // What the action map reads while input is held back, so held actions release and nothing new is pressed.
    input::Input idleInput;
    input::VirtualInput idleVirtualInput;

    FrameClock clock;
    FrameQueue frameQueue;
    EventBus events;
    TimerScheduler timers;
    TweenManager tweens;
    debug::Profiler profiler;
    std::unique_ptr<platform::Bridge> platform;
    std::unique_ptr<graphics2d::Renderer> renderer;
    std::unique_ptr<assets::Manager> assets;
    std::unique_ptr<SceneManager> scenes;
    plugins::PluginRegistry plugins;
    std::shared_ptr<text::Font> defaultFont;
    std::unique_ptr<varn::runtime::Runtime> runtime;
    std::unique_ptr<JobSystem> jobs;
    std::array<input::GamepadState, input::Input::kMaxGamepads> gamepads{};

    // The window, the keyboard, the network and the gamepads as the app last heard of them, so every change is published once. A connected gamepad keeps its name here so its disconnection can report it.
    bool fullscreen = false;
    platform::Orientation orientation = platform::Orientation::Landscape;
    math::Rect safeRect;
    std::optional<platform::SafeAreaSimulation> safeAreaSimulation;
    bool backLeavesApp = true;
    math::Rect keyboardFrame;
    Engine::NetworkState network = Engine::NetworkState::Unknown;
    std::array<std::optional<std::string>, input::Input::kMaxGamepads> gamepadNames;

    std::unique_ptr<ErrorScreen> errorScreen;
    Engine::AppState appState = Engine::AppState::Active;

    // An app in the foreground is active only while its window has the focus and no interruption of the system, such as a phone call, holds it.
    bool focused = true;
    bool interrupted = false;
    bool started = false;
    bool running = true;
    bool restartRequested = false;

    // Whether focus loss muted the master bus, and whether it was muted before, so focus coming back restores the choice of the player.
    bool focusMuted = false;
    bool mutedBeforeFocusLoss = false;
};

} // namespace haylen::core
