#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::plugins {

// Applies the changes of an app in development, even while the error screen shows: changed assets reload in place, and a changed Lua module, `app.json` or plugin manifest restarts the app. The changes come from the development session of the host, which scans the package folder on the I/O pool, so frames never wait for the file system. Apps that ship have no session, and the plugin stays idle for them.
class HotReloadPlugin final : public Plugin {
  public:
    static constexpr float kScanSeconds = 0.5F;

    explicit HotReloadPlugin(platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "hotReload";
    }
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Whether the app runs in development, where changes to its files reach it.
    [[nodiscard]] bool isActive() const noexcept {
        return session != nullptr;
    }

  private:
    static void apply(core::Engine& engine, const std::vector<std::string>& changed);

    platform::DevelopmentSession* session;
    float elapsed = 0.0F;
};

} // namespace haylen::plugins
