#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Installs `haylen.input`, which reads the keyboard, the mouse, touches, gamepads, gestures and the action map.
class InputPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "input";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
