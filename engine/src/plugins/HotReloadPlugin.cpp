#include "plugins/HotReloadPlugin.hpp"

#include <exception>
#include <memory>
#include <string>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/io/Path.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::plugins {

// Frames never wait for a scan, and the next scan starts once the interval passed and the last one finished.
void HotReloadPlugin::beginFrame(core::Engine& engine, float) {
    if (session == nullptr) {
        return;
    }
    elapsed += static_cast<float>(engine.getClock().getUnscaledDelta());
    if (elapsed >= kScanSeconds) {
        if (const std::shared_ptr<platform::DevelopmentSession::Scan> scan = session->beginScan()) {
            elapsed = 0.0F;
            engine.getJobs().postIo([scan] { scan->run(); });
        }
    }

    const std::vector<std::string> changed = session->takeChanges();
    if (!changed.empty()) {
        apply(engine, changed);
    }
}

// A change that restarts the app makes the rest of the batch moot, because the new app reads every file again.
void HotReloadPlugin::apply(core::Engine& engine, const std::vector<std::string>& changed) {
    std::vector<std::string> assets;
    for (const std::string& path : changed) {
        if (!io::PackageWatcher::isWatched(path)) {
            continue;
        }
        if (!io::Path::isInside(path, io::Path::kContentDirectory)) {
            core::Log::info("The file \"{}\" changed, restarting the app.", path);
            engine.requestRestart();
            return;
        }
        assets.push_back(path.substr(io::Path::kContentDirectory.size() + 1));
    }

    // An editor may still be writing a file when it is seen, so a failed reload waits for the next save instead of stopping the app.
    for (const std::string& asset : assets) {
        try {
            if (engine.getAssets().reload(asset) > 0) {
                core::Log::info("Reloaded \"{}\".", asset);
            }
        } catch (const std::exception& error) {
            core::Log::warning("The asset \"{}\" could not be reloaded yet: {}", asset, error.what());
        }
    }
}

} // namespace haylen::plugins
