#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/plugins/PluginRegistry.hpp"

struct lua_State;

namespace varn::runtime {
class Runtime;
}

namespace haylen::platform {
class Bridge;
class Host;
class Window;
struct Event;
} // namespace haylen::platform

namespace haylen::assets {
class Manager;
}

namespace haylen::audio {
class Mixer;
}

namespace haylen::debug {
class Profiler;
}

namespace haylen::graphics {
class Device;
class Viewport;
} // namespace haylen::graphics

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::text {
class Font;
}

namespace haylen::input {
class ActionMap;
class GestureRecognizer;
class Input;
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
    [[nodiscard]] graphics::Device& getGraphics() noexcept;
    [[nodiscard]] graphics2d::Renderer& getRenderer2D() noexcept;
    [[nodiscard]] audio::Mixer& getAudio() noexcept;
    [[nodiscard]] graphics::Viewport& getViewport() noexcept;
    [[nodiscard]] input::Input& getInput() noexcept;
    [[nodiscard]] input::GestureRecognizer& getGestures() noexcept;
    [[nodiscard]] input::ActionMap& getActions() noexcept;
    [[nodiscard]] input::VirtualInput& getVirtualInput() noexcept;
    [[nodiscard]] SceneManager& getScenes() noexcept;
    [[nodiscard]] assets::Manager& getAssets() noexcept;
    [[nodiscard]] platform::Bridge& getPlatform() noexcept;
    [[nodiscard]] plugins::PluginRegistry& getPlugins() noexcept;
    [[nodiscard]] const std::shared_ptr<text::Font>& getDefaultFont() noexcept;
    [[nodiscard]] varn::runtime::Runtime& getScriptRuntime() noexcept;
    [[nodiscard]] lua_State* getLuaState() noexcept;

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

    void quit();
    [[nodiscard]] bool isRunning() const noexcept;

    // Asks the runtime to start the app again from its package once this frame ends, which is how edited scripts reload.
    void requestRestart() noexcept;
    [[nodiscard]] bool isRestartRequested() const noexcept;

    // Pauses the game: pausable scenes, autoloads, timers and tweens stop, those that run when paused start, and fixed steps stop accumulating. It tells the scenes it stops or starts, emits pausedChanged and publishes the paused or unpaused event.
    void setPaused(bool value);
    [[nodiscard]] bool isPaused() const noexcept;

    [[nodiscard]] AppState getAppState() const noexcept;
    [[nodiscard]] NetworkState getNetworkState() const noexcept;

    // Returns whether the lifecycle options halt updates in the current app state. A halted app still delivers asynchronous results and queued events, and an app in the background never renders.
    [[nodiscard]] bool isHalted() const noexcept;
    [[nodiscard]] const AppConfig::Lifecycle& getLifecycle() const noexcept;
    void setLifecycle(const AppConfig::Lifecycle& value);

    // A simulated safe area replaces the one the device reports, to test layouts for other screens. The debug.safeArea option of app.json sets it at start.
    void setSafeAreaSimulation(std::optional<platform::SafeAreaSimulation> value);
    [[nodiscard]] const std::optional<platform::SafeAreaSimulation>& getSafeAreaSimulation() const noexcept;

    // Whether the back button of the platform, the Menu button of the Apple TV remote and the Back button of Android, leaves the app, which is right on the root screen. Otherwise the app keeps the press, which reaches it as ui_cancel, and an open UI popup always keeps it.
    void setBackLeavesApp(bool value) noexcept;
    [[nodiscard]] bool canBackLeaveApp() const noexcept;
    [[nodiscard]] bool isBackCaptured() const;

    Signal<const lua::Error&> errorRaised;
    Signal<bool> pausedChanged;
    Signal<AppState> appStateChanged;
    Signal<> resized;
    Signal<> quitRequested;

    // Fires when the platform reports memory pressure. Assets live only while something holds them, so this is when the app unloads preload groups and drops what it can rebuild.
    Signal<> lowMemory;

  private:
    void activatePlugin(plugins::Plugin& plugin);
    void publishDeviceChanges();
    void publishKeyboard(const math::Rect& value);
    void render(const std::vector<plugins::Plugin*>& all);
    void renderScenes(const std::vector<plugins::Plugin*>& all);
    void setAppState(AppState value);
    void applyFocusMute();
    [[nodiscard]] math::Insets getSafeAreaInsets() const;

    std::unique_ptr<EngineState> state;
};

} // namespace haylen::core
