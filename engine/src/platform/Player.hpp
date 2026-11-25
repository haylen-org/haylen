#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeHost.hpp"
#include "platform/ReportedState.hpp"

namespace haylen::gpu {
class Device;
}

namespace haylen::io {
class OverlayPackage;
class Package;
} // namespace haylen::io

namespace haylen::platform {

class DevelopmentSession;

// Plays apps on a real device for the platform layer that hosts them. It reads the command line, plays the package it names or the bundled one, replaces the running app whenever a package restarts it and hands the app the events of the window and the platform. The platform layer opens the window for the app that `getOpeningConfig` describes, installs its `NativeWindow` and calls `initialize`, `frame` and `cleanup` while the window lives.
class Player final {
  public:
    // Where a press of the back button of the platform goes: to the system, which leaves the app, to the player, which quits a launched app to the one that launched it, or to the app, which receives it as an event.
    enum class Back : std::uint8_t {
        System,
        Player,
        App,
    };

    explicit Player(std::unique_ptr<NativeHost> nativeHost);

    // Creates the player of the process for the command line with the host of the platform layer. A process that a link launched, with `--link`, hands the link to the instance of the app that runs already where the platform starts a process for every link, and then plays nothing, as `isLaunchForwarded` tells.
    static void create(int argc, char* argv[], std::unique_ptr<NativeHost> nativeHost);
    [[nodiscard]] static bool isLaunchForwarded() noexcept;

    // The configuration of the app that the window opens for.
    [[nodiscard]] static const core::AppConfig& getOpeningConfig() noexcept;

    // Where the window opens on the desktop, in desktop points, when `haylen.json` places it, which the platform layer opens it at.
    [[nodiscard]] static std::optional<math::Rect> getOpeningFrame();

    // The size in points that `haylen.json` gives the window the process opened with.
    [[nodiscard]] static math::Vec2 getOpeningSize() noexcept;

    // The graphics APIs the window tries in turn until one starts: the one that `--graphics-api` names, or else the ones of `window.graphicsApis` of `haylen.json`, then the other APIs of the build in the order of the platform.
    [[nodiscard]] static const std::vector<gpu::Device::Api>& getGraphicsApis() noexcept;

    // Starts the app once the window of the platform layer is open.
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

    // Whether the Lua debugger halts the app inside a frame, while the events that reach the player wait for the frames after the halt and the platform layer runs no frame.
    [[nodiscard]] static bool isHalted() noexcept;

    // Ends the running app and leaves an empty screen until the next restart.
    static void stop();

    // Ends the player of the process and closes its window, such as when the page that hosts the web runtime disposes of it, which no app follows.
    static void end();

    // Ends the running app with a failure that no app can recover from, such as a GPU that keeps no device for it. The reason reaches the log and the web page, and a window of the desktop closes with the exit status 1.
    static void fail(std::string_view reason);

    // Whether an app plays, which a stop or an app without its package ends.
    [[nodiscard]] static bool isPlaying() noexcept;

    // Pauses or resumes the running app. A paused app keeps its last frame on screen, and the engine sees it as suspended until it resumes.
    static void setPaused(bool value);
    [[nodiscard]] static bool isPaused() noexcept;

    // Hands the running app an event of the window or the platform, such as the keyboard and mouse of Mac Catalyst. It runs on the frame thread.
    static void handleEvent(const Event& event);

    // Routes a press or a release of the back button of the platform, which the platform layer leaves to the system, swallows or hands to the app as an event. It runs on the frame thread.
    [[nodiscard]] static Back routeBack(bool pressed);

    // Queues such an event from any thread, or from inside a call of the engine, such as the edits of a native text field. The running app receives it at the start of the next frame.
    static void postEvent(Event event);
    // Queues a press of the back button of the activity from any thread, which the next frame hands to the app or, in a launched app that does not take back, turns into a quit to the app that launched it.
    static void postBack();

    // Records from any thread whether the back button of the platform is down, which leaves a launched app once it is held long enough. A release returns whether it ends a hold that left a launched app, which the platform then swallows, so the app that starts again never hears it.
    [[nodiscard]] static bool holdBack(bool pressed);

    // Whether the running app takes the next press of the back button of the platform instead of leaving, as the engine decides from its app and its open popups.
    [[nodiscard]] static bool isBackCaptured();

    // The GPU device of the running app, which the platform layer tells when the surface of its window changes, or null while no app runs. It runs on the frame thread.
    [[nodiscard]] static gpu::Device* getGpuDevice() noexcept;

    // Whether the last frame of the running app drew and presented, whose present then paced the frame thread. A frame that draws nothing, such as one of an app that redraws on change while nothing changed, leaves the pace to the platform layer. It runs on the frame thread.
    [[nodiscard]] static bool isDrawing() noexcept;

    // Asks the running app for the next frame because the platform lost what its window showed, such as a window of a desktop that the system shows again without keeping its picture. It runs on the frame thread.
    static void requestRedraw() noexcept;

  private:
    static void prepare(int argc, char* argv[], std::unique_ptr<NativeHost> nativeHost);

    // Stands in for the app when its package cannot be opened or its `haylen.json` is invalid, so the window explains the problem instead of closing.
    class FailedApplication final : public core::Application {
      public:
        explicit FailedApplication(std::string reason) : error(std::move(reason)) {
            error.setCode(core::AppError::Code::Package);
        }

        void start(core::Engine&) override {
            throw error;
        }

      private:
        core::AppError error;
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

    // An app that launched the one that plays, which starts again from its package with its development session, its native access and its launch arguments when that app quits.
    struct Launcher {
        std::shared_ptr<io::Package> package;
        std::unique_ptr<DevelopmentSession> development;
        bool nativeAccess = true;
        std::vector<std::string> arguments;
    };

    // What outlives every player of the process: the player that plays, which the frame thread creates when the platform layer asks for the app and destroys when its window closes, the events other threads queue for it and the status the process exits with. It is never destroyed, because an exit() on another thread, such as the one the iOS simulator calls on a background queue when its render server dies, runs the destructors of static objects while the frame thread still plays the app.
    struct Process {
        std::unique_ptr<Player> current;
        std::mutex postedMutex;
        std::vector<Event> posted;
        int backPresses = 0;
        int exitStatus = 0;
        bool launchForwarded = false;
        std::atomic<bool> backHeld = false;
        std::atomic<bool> backSwallowed = false;
    };

    [[nodiscard]] static Process& getProcess() noexcept;
    [[nodiscard]] static Player& getCurrent() noexcept;

    // The execution budget of the development session, which times every call of the player into the engine, or null for an app that plays as it ships.
    [[nodiscard]] App load(const std::function<std::shared_ptr<io::Package>()>& open);
    [[nodiscard]] std::vector<gpu::Device::Api> resolveGraphicsApis(std::string_view requested, std::span<const gpu::Device::Api> configured);
    [[nodiscard]] static std::vector<Event> takePostedEvents();
    [[nodiscard]] static int takeBackPresses();
    void pressBack();

    [[nodiscard]] std::shared_ptr<io::Package> withOverlay(std::shared_ptr<io::Package> source) const;
    void play(double seconds);
    void endSplashScreenWhenDue();
    void launch();
    void leaveFailedStart();
    void launchWhenReady();
    void close() noexcept;
    void replace(App app);
    void enableDevelopment(std::optional<std::filesystem::path> folder, std::string server);
    void launchPackage(const core::Engine::Launch& request);
    void returnToLauncher();
    void deliver(const Event& event);

    std::unique_ptr<NativeHost> host;
    math::Vec2 openingSize;
    std::vector<gpu::Device::Api> graphicsApis;
    App pending;
    std::shared_ptr<io::Package> package;
    std::unique_ptr<core::Engine> engine;

    // The apps that launched the one that plays, the latest last.
    std::vector<Launcher> launchers;

    // What the platform reported last, which every new app hears when it starts, so an app that restarts in the background, such as under the activity of a screen on Android, starts there and draws nothing.
    ReportedState reported;
    bool playing = false;

    // Whether the window of the platform layer opened, from when on the player plays the apps that replace each other.
    bool initialized = false;

    // Whether the pending app waits for the platform to prepare its GPU device.
    bool waiting = false;

    // Whether the app took the last press of the back button, so its release goes to the same place.
    bool backCaptured = false;

    // When the player started, which the duration of the splash counts from, and whether the launch screen ended.
    std::chrono::steady_clock::time_point launched = std::chrono::steady_clock::now();
    bool splashEnded = false;
};

} // namespace haylen::platform
