#pragma once

#include <memory>
#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua {
class ModuleGraph;
class ModuleHotReload;
} // namespace haylen::lua

namespace haylen::lua::modules {

// Installs `haylen.hotReload` and, in development, gives the hot reload plugin the reload of the Lua modules of the app, whose module graph records every module that the app loads, until the app stops.
class HotReloadModule final : public Module {
  public:
    HotReloadModule();
    ~HotReloadModule() override;

    HotReloadModule(const HotReloadModule&) = delete;
    HotReloadModule& operator=(const HotReloadModule&) = delete;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.hotReload";
    }
    void install(core::Engine& engine, lua_State* L) override;
    void stop(core::Engine& engine) override;

    // The modules that the app loaded, which only an app in development records, or null.
    [[nodiscard]] ModuleGraph* getGraph() noexcept;

  private:
    std::unique_ptr<ModuleHotReload> reload;
};

} // namespace haylen::lua::modules
