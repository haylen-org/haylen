#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.jobs`, which starts Lua jobs that the script plugin runs within a time budget each frame.
class JobsModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.jobs";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
