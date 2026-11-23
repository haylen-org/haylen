#include "platform/headless/HeadlessPlayer.hpp"

#include <charconv>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Application.hpp"
#include "io/OverlayPackage.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::platform {

std::optional<HeadlessPlayer::Options> HeadlessPlayer::parse(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--") {
            options.arguments.assign(argv + index + 1, argv + argc);
            break;
        }

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

int HeadlessPlayer::run(const Options& options) {
    try {
        return play(options);
    } catch (const std::exception& exception) {
        core::Log::error("{}", exception.what());
        return 1;
    }
}

int HeadlessPlayer::play(const Options& options) {
    const std::shared_ptr<io::Package> package = io::Package::open(options.package);
    // The apps keep their user data in the folder the run names, or in a folder of the app under the temporary directory of the system.
    const std::filesystem::path dataDirectory = options.dataDirectory.value_or(std::filesystem::temp_directory_path() / "haylen-headless" / core::AppConfig::fromPackage(*package).identifier);
    HeadlessHost host(dataDirectory);
    host.setLaunchArguments(options.arguments);
    if (options.development) {
        host.enableDevelopment(package->getDirectory());
    }

    std::shared_ptr<io::Package> playing = package;
    std::unique_ptr<core::Engine> engine = launch(host, playing);

    // An app that another app launched starts that app again when it quits or cannot start, with the development session and the native access of that app, the way the player does.
    std::vector<Launcher> launchers;
    long long frame = 0;
    auto next = std::chrono::steady_clock::now();
    while (engine->isRunning() || !launchers.empty()) {
        if (!engine->isRunning()) {
            end(host, engine);
            engine = returnToLauncher(host, launchers, playing);
        }

        if (options.frames && frame >= *options.frames) {
            core::Log::error("The app did not quit within {} frames.", *options.frames);
            return 1;
        }

        if (engine->getError() != nullptr && !engine->isRecoverable()) {
            break;
        }

        if (engine->isRestartRequested()) {
            end(host, engine);
            engine = launch(host, playing);
        } else if (const std::optional<core::Engine::Launch> request = engine->getLaunchRequest()) {
            const std::span<const std::string> arguments = host.getLaunchArguments();
            launchers.push_back({.package = playing, .development = host.exchangeDevelopmentSession(nullptr), .nativeAccess = host.hasNativeAccess(), .arguments = {arguments.begin(), arguments.end()}});
            host.setLaunched(true);
            host.setLaunchArguments({});
            host.setNativeAccess(host.hasNativeAccess() && request->nativeAccess);
            end(host, engine);
            engine = launchPackage(host, *request, playing);
            if (engine == nullptr) {
                engine = returnToLauncher(host, launchers, playing);
            }
        }

        engine->frame(std::chrono::duration<double>(kFrameTime).count());

        ++frame;
        next += kFrameTime;
        std::this_thread::sleep_until(next);
    }

    const int status = host.getErrorReports().empty() ? engine->getExitStatus() : 1;
    end(host, engine);
    return status;
}

// A launched app that cannot start reports its failure, which makes the run fail, and returns no engine.
std::unique_ptr<core::Engine> HeadlessPlayer::launchPackage(HeadlessHost& host, const core::Engine::Launch& request, std::shared_ptr<io::Package>& playing) {
    try {
        playing = io::Package::open(request.package);
        if (!request.developmentServer.empty()) {
            host.enableDevelopment(std::nullopt, request.developmentServer);
            playing = std::make_shared<io::OverlayPackage>(std::move(playing));
        }

        return launch(host, playing);
    } catch (const std::exception& error) {
        core::Log::error("The launched app could not start: {}", error.what());
        host.reportError(core::AppError(error.what()).toJson());
        return nullptr;
    }
}

std::unique_ptr<core::Engine> HeadlessPlayer::returnToLauncher(HeadlessHost& host, std::vector<Launcher>& launchers, std::shared_ptr<io::Package>& playing) {
    Launcher launcher = std::move(launchers.back());
    launchers.pop_back();
    host.setLaunched(!launchers.empty());
    host.setNativeAccess(launcher.nativeAccess);
    host.setLaunchArguments(std::move(launcher.arguments));
    (void)host.exchangeDevelopmentSession(std::move(launcher.development));
    playing = std::move(launcher.package);
    return launch(host, playing);
}

std::unique_ptr<core::Engine> HeadlessPlayer::launch(HeadlessHost& host, const std::shared_ptr<io::Package>& package) {
    core::AppConfig config = core::AppConfig::fromPackage(*package);
    auto application = std::make_unique<lua::Application>();
    application->configure(config);
    host.setTransparencySupported(config.window.transparent);
    host.setWindowSampleCount(config.window.sampleCount);
    if (DevelopmentSession* session = host.getDevelopmentSession()) {
        session->setOverlay(std::dynamic_pointer_cast<io::OverlayPackage>(package));
    }

    auto engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
    engine->start();
    return engine;
}

void HeadlessPlayer::end(HeadlessHost& host, std::unique_ptr<core::Engine>& engine) {
    engine.reset();
}

} // namespace haylen::platform
