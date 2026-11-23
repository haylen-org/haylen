#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs the Lua modules of the engine core: `haylen`, `haylen.log`, `haylen.viewport`, `haylen.window`, `haylen.display`, `haylen.collections`, `haylen.timer`, `haylen.scene`, `haylen.signal`, `haylen.events`, `haylen.math`, `haylen.tween` and `haylen.ai`.
class CoreModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
