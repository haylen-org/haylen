#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/io/PackageWatcher.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Watches the package folder of an app in development. Changed files under source, a changed app.json and changed Lua modules and manifests of plugins restart the app, and changed files under content reload in place, even while the error screen shows. The folder is scanned on the I/O pool, so a large package never slows a frame down.
class HotReloadPlugin final : public Plugin {
  public:
    static constexpr float kScanSeconds = 0.5F;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "hotReload";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    [[nodiscard]] bool isWatching() const noexcept {
        return scan != nullptr;
    }

  private:
    // The watcher and the changes of its last scan, which the frame thread and one job on the I/O pool hand to each other through done. The first job creates the watcher, whose snapshot is where changes start.
    struct Scan {
        std::unique_ptr<io::PackageWatcher> watcher;
        std::vector<std::string> changed;
        std::atomic<bool> done = false;
    };

    static void apply(core::Engine& engine, const std::vector<std::string>& changed);

    std::shared_ptr<Scan> scan;
    float elapsed = 0.0F;
};

} // namespace haylen::plugins
