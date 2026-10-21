#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/AppPlugin.hpp"
#include "haylen/plugins/PluginRegistry.hpp"

struct lua_State;

namespace varn::runtime {
class Runtime;
}

namespace haylen::platform {
class Accessibility;
class Bridge;
class Dialogs;
class Host;
class MediaSession;
class Screens;
class System;
class Window;
struct Event;
} // namespace haylen::platform

namespace haylen::assets {
class Manager;
}

namespace haylen::content {
class ContentManager;
}

namespace haylen::audio {
class Mixer;
}

namespace haylen::debug {
class Profiler;
}

namespace haylen::graphics {
class Viewport;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::text {
class Font;
}

namespace haylen::input {
class ActionMap;
class ActionMaps;
class AppTextField;
class GestureRecognizer;
class Input;
class PointerEmulation;
class VirtualInput;
} // namespace haylen::input

namespace haylen::io {
class Package;
}

namespace haylen::storage {
class UserStorage;
}

namespace haylen::core {

class Application;
class EventBus;
class FrameClock;
class FrameQueue;
class JobSystem;
class SceneManager;
class TimerScheduler;
class TweenManager;
struct EngineState;

// Owns every engine service for one running app and drives the frame loop. A new engine is created to restart an app.
class Engine final {
  public:
    // Where the app stands with the platform: Active in the foreground with focus, Inactive while visible without focus or interrupted by the system, and Background while hidden.
    enum class AppState : std::uint8_t {
        Active,
        Inactive,
        Background,
    };

    // Whether the device reaches the network, as the platform last reported it. Browsers, Android and Apple platforms report it from the start, and the state stays Unknown on the others.
    enum class NetworkState : std::uint8_t {
        Unknown,
        Online,
        Offline,
    };

    // Where the code that may fail runs, which decides whether a fixed error can resume the app. Frame code, such as updates, renders, events, timers, tweens and listeners, runs again in the next frame, and a reload in development changed nothing when it failed, so both can resume. A lifecycle step, such as the start of the app, a scene hook that loads, enters, exits, unloads, pauses or resumes, the start or stop of an autoload, or a task that cannot come back, stopped half way, so only a restart starts it cleanly.
    enum class Phase : std::uint8_t {
        Frame,
        Lifecycle,
        Reload,
    };

    // Sets the phase for its lifetime and restores the previous one. An error that leaves a lifecycle scope counts as a lifecycle error wherever the engine reports it.
    class PhaseScope final {
      public:
        PhaseScope(Engine& owner, Phase value) noexcept;
        ~PhaseScope();

        PhaseScope(const PhaseScope&) = delete;
        PhaseScope& operator=(const PhaseScope&) = delete;

      private:
        Engine& engine;
        Phase previous;
        int exceptions;
    };

    // What the poll of the event loop of Varn did in the last frame: the time it could take, whether any job, timer or socket callback ran, whether work remains, and the milliseconds the loop can wait before it has work of its own, which is -1 when only work from outside can bring any.
    struct EventLoopPoll {
        std::chrono::nanoseconds budget{};
        bool ran = false;
        bool pending = false;
        long long idleMilliseconds = -1;
    };

    Engine(platform::Host& host, std::shared_ptr<io::Package> package, AppConfig config, std::unique_ptr<Application> application);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void start();
    void frame(double frameSeconds);
    void handleEvent(const platform::Event& event);
    void stop();

    [[nodiscard]] const AppConfig& getConfig() const noexcept;
    [[nodiscard]] platform::Window& getWindow() noexcept;
    [[nodiscard]] std::string_view getPlatformName() const noexcept;
    [[nodiscard]] io::Package& getPackage() noexcept;
    [[nodiscard]] storage::UserStorage& getStorage() noexcept;
    [[nodiscard]] JobSystem& getJobs() noexcept;
    [[nodiscard]] TimerScheduler& getTimers() noexcept;
    [[nodiscard]] TweenManager& getTweens() noexcept;
    [[nodiscard]] debug::Profiler& getProfiler() noexcept;
    [[nodiscard]] FrameClock& getClock() noexcept;
    [[nodiscard]] EventBus& getEvents() noexcept;
    [[nodiscard]] FrameQueue& getFrameQueue() noexcept;
    // Captures the next frame the app renders, everything the screen shows of it, into an image that reaches the callback in a later frame, read back as `graphics::Device::readTarget` reads.
    void captureScreen(graphics::Device::ReadCallback done);
    [[nodiscard]] graphics::Device& getGraphics() noexcept;
    [[nodiscard]] graphics2d::Renderer& getRenderer2D() noexcept;
    [[nodiscard]] audio::Mixer& getAudio() noexcept;
    [[nodiscard]] graphics::Viewport& getViewport() noexcept;
    [[nodiscard]] input::Input& getInput() noexcept;
    [[nodiscard]] input::GestureRecognizer& getGestures() noexcept;
    [[nodiscard]] input::ActionMap& getActions() noexcept;
    [[nodiscard]] input::ActionMaps& getActionMaps() noexcept;
    [[nodiscard]] input::VirtualInput& getVirtualInput() noexcept;
    [[nodiscard]] input::PointerEmulation& getPointerEmulation() noexcept;
    [[nodiscard]] input::AppTextField& getAppTextField() noexcept;
    [[nodiscard]] SceneManager& getScenes() noexcept;
    [[nodiscard]] assets::Manager& getAssets() noexcept;
    [[nodiscard]] content::ContentManager& getContent() noexcept;
    [[nodiscard]] platform::Bridge& getPlatform() noexcept;
    [[nodiscard]] platform::System& getSystem() noexcept;
    [[nodiscard]] platform::MediaSession& getMediaSession() noexcept;
    [[nodiscard]] platform::Accessibility& getAccessibility() noexcept;
    [[nodiscard]] platform::Dialogs& getDialogs() noexcept;
    [[nodiscard]] platform::Screens& getScreens() noexcept;
    [[nodiscard]] plugins::PluginRegistry& getPlugins() noexcept;
    [[nodiscard]] const std::shared_ptr<text::Font>& getDefaultFont() noexcept;
    [[nodiscard]] varn::runtime::Runtime& getScriptRuntime() noexcept;
    [[nodiscard]] lua_State* getLuaState() noexcept;
    [[nodiscard]] const EventLoopPoll& getEventLoopPoll() const noexcept;

    template <typename T> [[nodiscard]] T& getPlugin() {
        return getPlugins().get<T>();
    }

    // Registers a plugin and, when the engine is already running, starts it and installs its Lua modules right away.
    template <typename T> T& addPlugin(std::unique_ptr<T> plugin) {
        T& added = getPlugins().add(std::move(plugin));
        activatePlugin(added);
        return added;
    }

    // Stops app updates and shows the error screen until the app is restarted. Script errors end up here, and a lua::Error keeps its stack. Only the first error of an app counts.
    void reportError(const std::exception& exception);
    void reportError(const std::string& message);

    // Returns the error that stopped the app, or null while it runs.
    [[nodiscard]] const lua::Error* getError() const noexcept;

    // Whether the error that stopped the app came from code that runs again, so fixing it in development resumes the app instead of restarting it.
    [[nodiscard]] bool isErrorResumable() const noexcept;

    // Leaves the error screen at once when its error can resume, so the next frame updates and renders the app again with its state, and returns whether it did. Hot reload calls it once a reload fixed the code.
    bool clearError() noexcept;

    [[nodiscard]] Phase getPhase() const noexcept;

    // Lets the error screen offer to go back to the app instead of only restarting it, for apps that can return to a safe screen, such as a menu.
    void setRecoverable(bool value) noexcept;
    [[nodiscard]] bool isRecoverable() const noexcept;

    // Leaves the error screen at the start of the next frame, before the app updates: the engine publishes appRecovered with the error, whose listeners put the app in order, and the app runs on from there. Nothing happens while no error stops the app.
    void recover() noexcept;

    // Stops the app and asks the platform to close it. The status, 0 for success, is the exit status of the process on macOS, Windows and Linux, and the other platforms have none and ignore it.
    void quit(std::uint8_t status = 0);
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::uint8_t getExitStatus() const noexcept;

    // Keeps the app from starting until the returned hold ends, while the engine and its plugins run frames, such as while the content the app needs to start downloads. The app starts in the first frame after the last hold ends, or at once when no hold was taken before `start`.
    [[nodiscard]] std::shared_ptr<void> holdStart();

    // Whether the app waits for a hold before it starts.
    [[nodiscard]] bool isStartHeld() const noexcept;

    // Asks the runtime to start the app again from its package once this frame ends, which is how edited scripts reload.
    void requestRestart() noexcept;
    [[nodiscard]] bool isRestartRequested() const noexcept;

    // Asks the runtime to run the app package at the path, a folder or a zip file, in place of this app once this frame ends, and to start this app again when that app quits. Throws `std::invalid_argument` when nothing is at the path.
    void requestLaunch(const std::filesystem::path& package);
    [[nodiscard]] const std::optional<std::filesystem::path>& getLaunchRequest() const noexcept;

    // Whether another app launched this one, so that quitting it starts that app again.
    [[nodiscard]] bool isLaunched() const noexcept;

    // Pauses the game: pausable scenes, autoloads, timers and tweens stop, those that run when paused start, and fixed steps stop accumulating. It tells the scenes it stops or starts, emits pausedChanged and publishes the paused or unpaused event.
    void setPaused(bool value);
    [[nodiscard]] bool isPaused() const noexcept;

    [[nodiscard]] AppState getAppState() const noexcept;
    [[nodiscard]] NetworkState getNetworkState() const noexcept;

    // Whether native UI of plugins covers the app, such as a full screen ad, a sign-in form or the screen of a plugin. A covered app is inactive, halted and muted whatever the lifecycle options say, and it comes back as it was once the last cover ends. The engine takes the cover of the platform at the start of every frame. A covered app draws one frame once the cover began, which stays on screen, and draws it again only when the window changes size or the app comes back from the background, while it draws nothing at all under an opaque screen.
    [[nodiscard]] bool isAppCovered() const noexcept;

    // Returns whether the app is halted, by a cover or by the lifecycle options in the current app state. A halted app still delivers asynchronous results and queued events, and an app in the background never renders.
    [[nodiscard]] bool isHalted() const noexcept;
    [[nodiscard]] const AppConfig::Lifecycle& getLifecycle() const noexcept;
    void setLifecycle(const AppConfig::Lifecycle& value);

    // Change how the design space maps onto the screen while the app runs, starting from the design section of app.json. The viewport follows at once, and the UI, cameras and pointer input with it.
    void setScaling(graphics::Viewport::ScalingPolicy value);
    void setDesignSize(math::Vec2 value);

    // A simulated safe area replaces the one the device reports, to test layouts for other screens. The debug.safeArea option of app.json sets it at start.
    void setSafeAreaSimulation(std::optional<platform::SafeAreaSimulation> value);
    [[nodiscard]] const std::optional<platform::SafeAreaSimulation>& getSafeAreaSimulation() const noexcept;

    // The fold of a foldable or dual-screen device across the window in framebuffer pixels, the simulated one while a simulation replaces the fold that the device reports, or none. The debug.fold option of app.json sets the simulation at start, and the windowFoldChanged event announces every change of the fold or of the posture.
    [[nodiscard]] std::optional<platform::Fold> getFold() const;
    void setFoldSimulation(std::optional<platform::FoldSimulation> value);
    [[nodiscard]] const std::optional<platform::FoldSimulation>& getFoldSimulation() const noexcept;

    // The screen edges that native views of plugins reserve, such as a banner ad, as the largest reservation on each edge in framebuffer pixels. The safe area of the viewport grows on each edge to cover them, so UI anchored to the safe area moves out of their way. The engine takes them at the start of every frame.
    [[nodiscard]] const math::Insets& getReservedInsets() const noexcept;

    // The plugins that app.json lists, in the order of their ids, with the version and parameter defaults of their plugin.json and whether their native part runs on this platform. Throws std::runtime_error when a plugin.json of the package cannot be read.
    [[nodiscard]] std::vector<platform::AppPlugin> getAppPlugins() const;

    // The ids of the plugins whose native part runs, which the platform loaded or a native library of the app declared, in order.
    [[nodiscard]] std::vector<std::string> getNativePlugins() const;

    // Whether the back button of the platform, the Menu button of the Apple TV remote and the Back button of Android, leaves the app, which is right on the root screen. Otherwise the app keeps the press, which reaches it as uiCancel, and an open UI popup always keeps it.
    void setBackLeavesApp(bool value) noexcept;
    [[nodiscard]] bool canBackLeaveApp() const noexcept;
    [[nodiscard]] bool isBackCaptured() const;

    // The color that the engine paints behind the status bar while it shows, over everything the app drew there, or none to leave what the app drew. Only the visible area of the viewport takes it, so the clear color shows where letterbox bars cover the top of the screen.
    void setStatusBarColor(std::optional<math::Color> value) noexcept;
    [[nodiscard]] std::optional<math::Color> getStatusBarColor() const noexcept;

    Signal<const lua::Error&> errorRaised;
    Signal<bool> pausedChanged;
    Signal<AppState> appStateChanged;
    Signal<> resized;
    Signal<> quitRequested;

    // Fires when the platform reports memory pressure. Assets live only while something holds them, so this is when the app unloads preload groups and drops what it can rebuild.
    Signal<> lowMemory;

  private:
    // Holds the one engine of the process while it lives, because the GPU device, the renderer and the interface keep state of the process that one engine at a time owns.
    class Claim final {
      public:
        Claim();
        ~Claim();
        Claim(const Claim&) = delete;
        Claim& operator=(const Claim&) = delete;

      private:
        [[nodiscard]] static std::atomic<bool>& getClaimed() noexcept;
    };

    // The event loop takes a share of the time of a frame, within bounds, so its tasks and sockets advance every frame without ever holding one up.
    static constexpr double kPollBudgetShare = 0.25;
    static constexpr std::chrono::nanoseconds kMinimumPollBudget = std::chrono::milliseconds(1);
    static constexpr std::chrono::nanoseconds kMaximumPollBudget = std::chrono::milliseconds(8);

    [[nodiscard]] static std::chrono::nanoseconds getPollBudget(double frameSeconds) noexcept;
    void activatePlugin(plugins::Plugin& plugin);
    void startApplication();
    void applyWindowOptions();
    void leaveErrorScreen();
    void publishDeviceChanges();
    void publishKeyboard(const math::Rect& value);
    [[nodiscard]] Json describeFold(const std::optional<platform::Fold>& fold) const;

    // A rectangle of framebuffer pixels as the table of design units that events carry.
    [[nodiscard]] Json toDesignJson(const math::Rect& pixels) const;
    void dispatchEvent(const platform::Event& event);
    void render(std::size_t plugins);
    void paintStatusBar(math::Color color);
    void renderScenes(std::size_t plugins);
    void setAppState(AppState value);
    void releaseHeldInput();
    void refreshForegroundState();
    [[nodiscard]] AppState getForegroundState() const noexcept;
    void applyCover();
    void applyStateMute();
    void remapViewport();
    [[nodiscard]] math::Insets getSafeAreaInsets() const;

    Claim claim;
    std::unique_ptr<EngineState> state;
};

} // namespace haylen::core
