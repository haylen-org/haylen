#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

namespace haylen::core {
class Engine;
}

namespace haylen::io {
class Package;
}

namespace haylen::platform {

class HeadlessHost;

// Runs an app package without a window, GPU or audio device on the headless host, at 60 frames per second of real time, so asynchronous work finishes the way it does on a device. The app runs until it quits, until an error stops an app that is not recoverable, or for the number of frames that `--frames` allows, and `--dev` runs it in development with hot reload of its folder, like the player. The exit status is the status the app quit with, or 1 when an error was reported or the app did not quit in time, which suits automatic runs such as continuous integration.
class HeadlessPlayer final {
  public:
    struct Options {
        std::filesystem::path package;
        std::optional<long long> frames;
        bool development = false;
    };

    static constexpr std::chrono::nanoseconds kFrameTime{16'666'667};

    [[nodiscard]] static std::optional<Options> parse(int argc, char** argv);

    // A package that cannot open or an app that cannot start stops the run with its message.
    static int run(const Options& options);

  private:
    static int play(const Options& options);
    [[nodiscard]] static std::unique_ptr<core::Engine> launch(HeadlessHost& host, const std::shared_ptr<io::Package>& package);
};

} // namespace haylen::platform
