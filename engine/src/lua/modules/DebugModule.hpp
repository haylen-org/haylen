#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.debug`, with the statistics, the debug drawings, the traces and the captures of the app.
class DebugModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.debug";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
