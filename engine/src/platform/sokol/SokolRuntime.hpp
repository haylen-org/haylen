#pragma once

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
#include "haylen/core/Connection.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolHost.hpp"
#include "sokol_app.h"

namespace haylen::core {
class Engine;
}

namespace haylen::io {
class Package;
}

namespace haylen::platform {

// Plays apps with sokol_app. It opens the window for the package that the command line names, or the bundled one, and replaces the running app whenever a package restarts it.
class SokolRuntime final {
  public:
#if defined(__APPLE__)
    // Plays the command line until the window closes. Apple apps enter here through haylen_main, because sokol_app leaves main to them.
    static int run(int argc, char* argv[]);
#endif

    // Prepares the runtime for the command line and describes the window of the app to sokol_app.
    [[nodiscard]] static sapp_desc describe(int argc, char* argv[]);

    // Replaces the running app with another package, as if the app had started with it. The window stays open, and a package that fails to open or load shows its error on screen.
    static void restart(std::shared_ptr<io::Package> source);
    static void restart(const std::function<std::shared_ptr<io::Package>()>& open);

    // Starts the last playing app again from its package, which reloads every script.
    static void restart();

    // Ends the running app and leaves an empty screen until the next restart.
    static void stop();

    // Pauses or resumes the running app. A paused app keeps its last frame on screen, and the engine sees it as suspended until it resumes.
    static void setPaused(bool value);
    [[nodiscard]] static bool isPaused() noexcept;

    // Reloads the cached assets read from a changed file, a path relative to the content folder, and returns whether any were loaded.
    static bool reloadAsset(std::string_view path);

    // Hands the running app an event that reaches the platform outside sokol_app, such as the keyboard and mouse of Mac Catalyst. It runs on the frame thread.
    static void handleEvent(const Event& event);

    // Queues such an event from any thread, or from inside a call of the engine, such as the edits of a native text field. The running app receives it at the start of the next frame.
    static void postEvent(Event event);

    // Whether the running app takes the next press of the back button of the platform instead of leaving, as the engine decides from its app and its open popups.
    [[nodiscard]] static bool isBackCaptured();

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

    // The command line names the package to play, or none for the bundled one, and --dev turns on development behavior such as hot reload. Shipped apps never pass it.
    // The package to play, whether it plays in development, and the folders where native.load looks first, such as the one make.py builds the libraries of an app into.
    struct LaunchOptions {
        std::string package;
        bool development = false;
        std::vector<std::string> nativeFolders;
    };

    // What outlives every runtime of the process: the runtime that plays, which the frame thread creates when sokol_app asks for the app and destroys in the cleanup of sokol_app, and the events other threads queue for it. It is never destroyed, because an exit() on another thread, such as the one the iOS simulator calls on a background queue when its render server dies, runs the destructors of static objects while the frame thread still plays the app.
    struct Process {
        std::unique_ptr<SokolRuntime> current;
        std::mutex postedMutex;
        std::vector<Event> posted;
    };

    [[nodiscard]] static Process& getProcess() noexcept;
    [[nodiscard]] static SokolRuntime& getCurrent() noexcept;
    [[nodiscard]] static LaunchOptions parseLaunchOptions(int argc, char* argv[]);
    [[nodiscard]] static App load(const std::function<std::shared_ptr<io::Package>()>& open, bool hotReload);
    [[nodiscard]] static std::vector<Event> takePostedEvents();

    static void onInitialize(void* data);
    static void onFrame(void* data);
    static void onEvent(const sapp_event* source, void* data);
    static void onCleanup(void* data);
#if defined(__ANDROID__)
    static bool onAndroidInput(const void* source);

    // From Android 13 on, back reaches the app only through a callback of the activity, which exists only while the app takes back or edits a text field, so the back animation of the system plays only when back leaves the app.
    void reportBackCapture();
#endif

    void describeDesktop(sapp_desc& desc, const core::AppConfig::Window& window);
    void launch();
    void close() noexcept;
    void replace(App app);
    void deliver(const Event& event);

    SokolHost host;
    App pending;
    std::shared_ptr<io::Package> package;
    std::unique_ptr<core::Engine> engine;
    core::Connection errors;

    // The last network state the platform reported, which every new app hears when it starts.
    std::optional<bool> online;
    bool development = false;
    bool playing = false;
    bool paused = false;

    // Whether the app took the last press of the back button, so its release goes to the same place.
    bool backCaptured = false;
#if defined(__ANDROID__)
    // Whether the activity last heard that the app takes the back button. Each activity creates its own runtime and starts without a back callback.
    bool backReported = false;
#endif
#if defined(__EMSCRIPTEN__)
    std::string canvas;
#endif
};

} // namespace haylen::platform
