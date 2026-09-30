#include "haylen/plugins/HotReloadPlugin.hpp"

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::plugins {

void HotReloadPlugin::start(core::Engine& engine) {
    const std::optional<std::filesystem::path> folder = engine.getPackage().getDirectory();
    if (!engine.getConfig().hotReload || !folder) {
        return;
    }
    scan = std::make_shared<Scan>();
    // clang-format off
    engine.getJobs().postIo([current = scan, root = *folder] {
        current->watcher = std::make_unique<io::PackageWatcher>(root);
        current->done.store(true, std::memory_order_release);
        core::Log::info("Watching \"{}\" for changes.", root.generic_string());
    });
    // clang-format on
}

void HotReloadPlugin::stop(core::Engine&) {
    scan.reset();
    elapsed = 0.0F;
}

// Frames never wait for a scan, and the next scan starts once the interval passed after the last one finished.
void HotReloadPlugin::beginFrame(core::Engine& engine, float) {
    if (!scan || !scan->done.load(std::memory_order_acquire)) {
        return;
    }
    if (!scan->changed.empty()) {
        apply(engine, std::exchange(scan->changed, {}));
    }

    elapsed += static_cast<float>(engine.getClock().getUnscaledDelta());
    if (elapsed < kScanSeconds) {
        return;
    }
    elapsed = 0.0F;
    scan->done.store(false, std::memory_order_relaxed);
    // clang-format off
    engine.getJobs().postIo([current = scan] {
        current->changed = current->watcher->scan();
        current->done.store(true, std::memory_order_release);
    });
    // clang-format on
}

void HotReloadPlugin::apply(core::Engine& engine, const std::vector<std::string>& changed) {
    for (const std::string& path : changed) {
        // An editor may still be writing a file when it is seen, so a failed reload waits for the next save instead of stopping the app.
        if (io::Path::isInside(path, io::Path::kContentDirectory)) {
            const std::string asset = path.substr(io::Path::kContentDirectory.size() + 1);
            try {
                if (engine.getAssets().reload(asset) > 0) {
                    core::Log::info("Reloaded \"{}\".", asset);
                }
            } catch (const std::exception& error) {
                core::Log::warning("The asset \"{}\" could not be reloaded yet: {}", asset, error.what());
            }
            continue;
        }
        if (io::Path::isInside(path, io::Path::kSourceDirectory) || io::Path::isInside(path, io::Path::kPluginsDirectory) || path == io::Path::kAppConfigFile) {
            core::Log::info("The file \"{}\" changed, restarting the app.", path);
            engine.requestRestart();
        }
    }
}

} // namespace haylen::plugins
