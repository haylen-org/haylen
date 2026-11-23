#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.hotReload` and, in development, the module graph of the hot reload plugin, which records the modules that the app loads.
class HotReloadModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.hotReload";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
