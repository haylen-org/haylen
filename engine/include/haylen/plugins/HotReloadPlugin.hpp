#pragma once

#include <optional>
#include <string_view>

#include "haylen/io/PackageWatcher.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Watches the package folder of an app in development. Changed files under source and a changed app.json restart the app, and changed files under content reload in place, even while the error screen shows.
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
        return watcher.has_value();
    }

  private:
    std::optional<io::PackageWatcher> watcher;
    float elapsed = 0.0F;
};

} // namespace haylen::plugins
