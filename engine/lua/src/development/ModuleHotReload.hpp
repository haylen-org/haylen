#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "development/ModuleGraph.hpp"
#include "haylen/core/AppError.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"

struct lua_State;

namespace haylen::lua {

// Reloads the Lua modules of an app in development for the hot reload plugin: its module graph replaces `require` of the Lua state, so it records every module that the app loads, and a changed module reloads in place with the state of the app, in the order the modules require each other.
class ModuleHotReload final : public haylen::plugins::HotReloadPlugin::Scripts {
  public:
    explicit ModuleHotReload(lua_State* L);

    [[nodiscard]] bool isLoaded(std::string_view path) const override;
    [[nodiscard]] std::string findRestartReason(haylen::core::Engine& engine, const std::string& path) const override;
    void remove(haylen::core::Engine& engine, const std::string& path) override;
    [[nodiscard]] bool reload(haylen::core::Engine& engine, const std::vector<std::string>& paths, haylen::plugins::HotReloadPlugin::Report& report, std::string& restartFile) override;

    // The modules that the app loaded, with the values they keep across reloads.
    [[nodiscard]] ModuleGraph& getGraph() noexcept {
        return graph;
    }

  private:
    // Calls the `reloaded` hooks and returns the error of the first one that failed.
    [[nodiscard]] std::optional<haylen::core::AppError> callHooks(haylen::core::Engine& engine, const std::vector<std::string>& paths, int instances) const;

    lua_State* state;
    ModuleGraph graph;
};

} // namespace haylen::lua
