#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.input`, with the action maps, the devices, the gestures and the virtual controls.
class InputModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.input";
    }
    void install(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
