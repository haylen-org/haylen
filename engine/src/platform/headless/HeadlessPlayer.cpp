#include <charconv>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::platform {

// Runs an app package without a window, GPU or audio device on the headless host, at 60 frames per second of real time, so asynchronous work finishes the way it does on a device. The app runs until it quits, until an error stops an app that is not recoverable, or for the number of frames that `--frames` allows, and `--dev` runs it in development with hot reload of its folder, like the player. The exit status is 0 when the app quit and no error was reported, and 1 otherwise, which suits automatic runs such as continuous integration.
class HeadlessPlayer final {
  public:
    struct Options {
        std::filesystem::path package;
        std::optional<long long> frames;
        bool development = false;
    };

    static constexpr std::chrono::nanoseconds kFrameTime{16'666'667};

    [[nodiscard]] static std::optional<Options> parse(int argc, char** argv) {
        Options options;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument == "--dev") {
                options.development = true;
            } else if (argument == "--frames" && index + 1 < argc) {
                const std::string_view count = argv[++index];
                long long value = 0;
                if (std::from_chars(count.data(), count.data() + count.size(), value).ptr != count.data() + count.size() || value <= 0) {
                    return std::nullopt;
                }
                options.frames = value;
            } else if (options.package.empty() && !argument.starts_with("--")) {
                options.package = argument;
            } else {
                return std::nullopt;
            }
        }
        if (options.package.empty()) {
            return std::nullopt;
        }
        return options;
    }

    // A package that cannot open or an app that cannot start stops the run with its message.
    static int run(const Options& options) {
        try {
            return play(options);
        } catch (const std::exception& exception) {
            core::Log::error("{}", exception.what());
            return 1;
        }
    }

  private:
    static int play(const Options& options) {
        const std::shared_ptr<io::Package> package = io::Package::open(options.package);
        HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-headless" / core::AppConfig::fromPackage(*package).identifier);
        if (options.development) {
            host.enableDevelopment(package->getDirectory());
        }
        std::unique_ptr<core::Engine> engine = launch(host, package);

        long long frame = 0;
        auto next = std::chrono::steady_clock::now();
        while (engine->isRunning()) {
            if (options.frames && frame >= *options.frames) {
                core::Log::error("The app did not quit within {} frames.", *options.frames);
                return 1;
            }
            if (engine->getError() != nullptr && !engine->isRecoverable()) {
                break;
            }
            if (engine->isRestartRequested()) {
                engine.reset();
                engine = launch(host, package);
            }

            engine->frame(std::chrono::duration<double>(kFrameTime).count());
            ++frame;
            next += kFrameTime;
            std::this_thread::sleep_until(next);
        }

        const bool failed = !host.getErrorReports().empty();
        engine->stop();
        return failed ? 1 : 0;
    }

    [[nodiscard]] static std::unique_ptr<core::Engine> launch(HeadlessHost& host, const std::shared_ptr<io::Package>& package) {
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        host.setTransparencySupported(config.window.transparent);
        auto engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
        engine->start();
        return engine;
    }
};

} // namespace haylen::platform

int main(int argc, char** argv) {
    const std::optional<haylen::platform::HeadlessPlayer::Options> options = haylen::platform::HeadlessPlayer::parse(argc, argv);
    if (!options) {
        std::fprintf(stderr, "Usage: \"haylen-headless <app folder or zip> [--frames <count>] [--dev]\".\n");
        return 2;
    }
    return haylen::platform::HeadlessPlayer::run(*options);
}
