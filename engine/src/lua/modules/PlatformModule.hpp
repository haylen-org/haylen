#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.platform`, `haylen.system` and `haylen.accessibility`.
class PlatformModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.platform";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
