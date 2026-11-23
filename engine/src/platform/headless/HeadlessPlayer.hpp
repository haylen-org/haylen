#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::platform {

class DevelopmentSession;
class HeadlessHost;

// Runs an app package without a window, GPU or audio device on the headless host, at 60 frames per second of real time, so asynchronous work finishes the way it does on a device. The app runs until it quits, until an error stops an app that is not recoverable, or for the number of frames that `--frames` allows, `--dev` runs it in development with hot reload of its folder, like the player, and every argument after `--` reaches the app as its launch arguments. The exit status is the status the app quit with, or 1 when an error was reported or the app did not quit in time, which suits automatic runs such as continuous integration.
class HeadlessPlayer final {
  public:
    struct Options {
        std::filesystem::path package;
        std::optional<long long> frames;
        bool development = false;
        std::optional<std::filesystem::path> dataDirectory;
        std::vector<std::string> arguments;
    };

    static constexpr std::chrono::nanoseconds kFrameTime{16'666'667};

    [[nodiscard]] static std::optional<Options> parse(int argc, char** argv);

    // A package that cannot open or an app that cannot start stops the run with its message, while a launched app that cannot start fails the run and starts the app that launched it again.
    static int run(const Options& options);

  private:
    // An app that launched the one that plays, which starts again from its package with its development session, its native access and its launch arguments when that app quits.
    struct Launcher {
        std::shared_ptr<io::Package> package;
        std::unique_ptr<DevelopmentSession> development;
        bool nativeAccess = true;
        std::vector<std::string> arguments;
    };

    static int play(const Options& options);
    [[nodiscard]] static std::unique_ptr<core::Engine> launchPackage(HeadlessHost& host, const core::Engine::Launch& request, std::shared_ptr<io::Package>& playing);
    [[nodiscard]] static std::unique_ptr<core::Engine> returnToLauncher(HeadlessHost& host, std::vector<Launcher>& launchers, std::shared_ptr<io::Package>& playing);
    [[nodiscard]] static std::unique_ptr<core::Engine> launch(HeadlessHost& host, const std::shared_ptr<io::Package>& package);

    // Ends the app of the engine, whose stop the execution budget of development times like every other call into the engine.
    static void end(HeadlessHost& host, std::unique_ptr<core::Engine>& engine);
};

} // namespace haylen::platform
