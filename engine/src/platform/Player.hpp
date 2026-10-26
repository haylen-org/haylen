#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeHost.hpp"
#include "platform/ReportedState.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::gpu {
class Device;
}

namespace haylen::io {
class OverlayPackage;
class Package;
} // namespace haylen::io

namespace haylen::platform {

class DevelopmentSession;

// Plays apps on a real device for the backend that hosts them. It reads the command line, plays the package it names or the bundled one, replaces the running app whenever a package restarts it and hands the app the events of the window and the platform. The backend opens the window for the app that `getOpeningConfig` describes, installs its `NativeWindow` and calls `initialize`, `frame` and `cleanup` while the window lives.
class Player final {
  public:
    // Where a press of the back button of the platform goes: to the system, which leaves the app, to the player, which quits a launched app to the one that launched it, or to the app, which receives it as an event.
    enum class Back : std::uint8_t {
        System,
        Player,
        App,
    };

    explicit Player(std::unique_ptr<NativeHost> nativeHost);

    // Creates the player of the process for the command line with the host of the backend.
    static void create(int argc, char* argv[], std::unique_ptr<NativeHost> nativeHost);

    // The configuration of the app that the window opens for.
    [[nodiscard]] static const core::AppConfig& getOpeningConfig() noexcept;

    // Where the window opens on the desktop, in desktop points, when `app.json` places it, which the backend opens it at.
    [[nodiscard]] static std::optional<math::Rect> getOpeningFrame();

    // The size in points that `app.json` gives the window the process opened with.
    [[nodiscard]] static math::Vec2 getOpeningSize() noexcept;

    // The graphics APIs the window tries in turn until one starts: the one that `--graphics-api` names, or else the ones of `window.graphicsApis` of `app.json`, then the other APIs of the build in the order of the platform.
    [[nodiscard]] static const std::vector<gpu::Device::Api>& getGraphicsApis() noexcept;

    // Starts the app once the window of the backend is open.
    static void initialize();

    // Plays one frame of the app, the seconds since the last one.
    static void frame(double seconds);

    // Ends the app as the window closes.
    static void cleanup();

    // The status that the last app quit with, or 1 when the engine could not start, which desktop processes exit with.
    [[nodiscard]] static int getExitStatus() noexcept;

    // Replaces the running app with another package, as if the app had started with it. The window stays open, and a package that fails to open or load shows its error on screen.
    static void restart(std::shared_ptr<io::Package> source);
    static void restart(const std::function<std::shared_ptr<io::Package>()>& open);

    // Starts the last playing app again from its package, which reloads every script.
    static void restart();

    // Stops the running app and makes the package the one that `restart` starts, such as an empty package that a page fills file by file.
    static void stage(std::shared_ptr<io::Package> source);

    // The overlay of the package that `restart` starts, whose files a page replaces. Throws for a protected release, which plays without one.
    [[nodiscard]] static io::OverlayPackage& getOverlay();

    // The development session of the process, or null when it plays the app the way it ships.
    [[nodiscard]] static DevelopmentSession* getDevelopmentSession() noexcept;

    // Ends the running app and leaves an empty screen until the next restart.
    static void stop();

    // Pauses or resumes the running app. A paused app keeps its last frame on screen, and the engine sees it as suspended until it resumes.
    static void setPaused(bool value);
    [[nodiscard]] static bool isPaused() noexcept;

    // Hands the running app an event of the window or the platform, such as the keyboard and mouse of Mac Catalyst. It runs on the frame thread.
    static void handleEvent(const Event& event);

    // Routes a press or a release of the back button of the platform, which the backend leaves to the system, swallows or hands to the app as an event. It runs on the frame thread.
    [[nodiscard]] static Back routeBack(bool pressed);

    // Queues such an event from any thread, or from inside a call of the engine, such as the edits of a native text field. The running app receives it at the start of the next frame.
    static void postEvent(Event event);
    // Queues a press of the back button of the activity from any thread, which the next frame hands to the app or, in a launched app that does not take back, turns into a quit to the app that launched it.
    static void postBack();

    // Whether the running app takes the next press of the back button of the platform instead of leaving, as the engine decides from its app and its open popups.
    [[nodiscard]] static bool isBackCaptured();

    // The GPU device of the running app, which the backend tells when the surface of its window changes, or null while no app runs. It runs on the frame thread.
    [[nodiscard]] static gpu::Device* getGpuDevice() noexcept;

  private:
    // Stands in for the app when its package cannot be opened, so the window explains the problem instead of closing.
    class FailedApplication final : public core::Application {
      public:
        explicit FailedApplication(std::string reason) : message(std::move(reason)) {}

        void start(core::Engine&) override {
            throw std::runtime_error(message);
        }

      private:
        std::string message;
    };

    // Keeps the window alive with an empty screen after the page stops the app.
    class StoppedApplication final : public core::Application {
      public:
        void start(core::Engine&) override {}
    };

    struct App {
        std::shared_ptr<io::Package> package;
        core::AppConfig config;
        std::unique_ptr<core::Application> application;
        bool playing = true;
    };

    // The package to play, or none for the bundled one, whether it plays in development, which only --dev turns on and shipped apps never pass, the address of the development server that --dev-server gives, the folders where native.load looks first, such as the one haylen.py builds the libraries of an app into, and the graphics API that --graphics-api names, which the web page passes the same way.
    struct LaunchOptions {
        std::string package;
        bool development = false;
        std::string developmentServer;
        std::vector<std::string> nativeFolders;
        std::string graphicsApi;
    };

    // What outlives every player of the process: the player that plays, which the frame thread creates when the backend asks for the app and destroys when its window closes, the events other threads queue for it and the status the process exits with. It is never destroyed, because an exit() on another thread, such as the one the iOS simulator calls on a background queue when its render server dies, runs the destructors of static objects while the frame thread still plays the app.
    struct Process {
        std::unique_ptr<Player> current;
        std::mutex postedMutex;
        std::vector<Event> posted;
        int backPresses = 0;
        int exitStatus = 0;
    };

    [[nodiscard]] static Process& getProcess() noexcept;
    [[nodiscard]] static Player& getCurrent() noexcept;
    [[nodiscard]] static LaunchOptions parseLaunchOptions(int argc, char* argv[]);
    [[nodiscard]] App load(const std::function<std::shared_ptr<io::Package>()>& open);
    [[nodiscard]] static std::vector<Event> takePostedEvents();
    [[nodiscard]] static int takeBackPresses();
    void pressBack();

#if defined(__ANDROID__)
    // Back reaches the app only through the back callback of the activity, which is enabled only while the app takes back or edits a text field, so the predictive back animation of the system plays only when back leaves the app.
    void reportBackCapture();
#endif

    [[nodiscard]] std::shared_ptr<io::Package> withOverlay(std::shared_ptr<io::Package> source) const;
    void endSplashScreenWhenDue();
    void launch();
    void close() noexcept;
    void replace(App app);
    void launchPackage(const std::filesystem::path& path);
    void returnToLauncher();
    void deliver(const Event& event);

    std::unique_ptr<NativeHost> host;
    math::Vec2 openingSize;
    std::vector<gpu::Device::Api> graphicsApis;
    App pending;
    std::shared_ptr<io::Package> package;
    std::unique_ptr<core::Engine> engine;

    // The packages of the apps that launched the one that plays, the latest last, each of which starts again when the app it launched quits.
    std::vector<std::shared_ptr<io::Package>> launchers;

    // What the platform reported last, which every new app hears when it starts, so an app that restarts in the background, such as under the activity of a screen on Android, starts there and draws nothing.
    ReportedState reported;
    bool playing = false;

    // Whether the window of the backend opened, from when on the player plays the apps that replace each other.
    bool initialized = false;

    // Whether the app took the last press of the back button, so its release goes to the same place.
    bool backCaptured = false;

    // When the player started, which the duration of the splash counts from, and whether the launch screen ended.
    std::chrono::steady_clock::time_point launched = std::chrono::steady_clock::now();
    bool splashEnded = false;
#if defined(__ANDROID__)
    // Whether the activity last heard that the app takes the back button. Each activity creates its own player and starts with its back callback disabled.
    bool backReported = false;
#endif
};

} // namespace haylen::platform
