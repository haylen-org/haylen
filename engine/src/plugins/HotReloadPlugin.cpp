#include "haylen/plugins/HotReloadPlugin.hpp"

#include <exception>
#include <string>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::plugins {

void HotReloadPlugin::start(core::Engine& engine) {
    const std::optional<std::filesystem::path> folder = engine.getPackage().getDirectory();
    if (engine.getConfig().hotReload && folder) {
        watcher.emplace(*folder);
        core::Log::info("Watching {} for changes.", folder->generic_string());
    }
}

void HotReloadPlugin::stop(core::Engine&) {
    watcher.reset();
}

void HotReloadPlugin::beginFrame(core::Engine& engine, float) {
    if (!watcher) {
        return;
    }
    elapsed += static_cast<float>(engine.getClock().getUnscaledDelta());
    if (elapsed < kScanSeconds) {
        return;
    }
    elapsed = 0.0F;

    for (const std::string& path : watcher->scan()) {
        // An editor may still be writing a file when it is seen, so a failed reload waits for the next save instead of stopping the app.
        if (io::Path::isInside(path, io::Path::kContentDirectory)) {
            const std::string asset = path.substr(io::Path::kContentDirectory.size() + 1);
            try {
                if (engine.getAssets().reload(asset) > 0) {
                    core::Log::info("Reloaded {}.", asset);
                }
            } catch (const std::exception& error) {
                core::Log::warning("{} could not be reloaded yet: {}", asset, error.what());
            }
            continue;
        }
        if (io::Path::isInside(path, io::Path::kSourceDirectory) || path == io::Path::kAppConfigFile) {
            core::Log::info("{} changed, restarting the app.", path);
            engine.requestRestart();
        }
    }
}

} // namespace haylen::plugins
