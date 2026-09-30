#pragma once

#include <memory>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class NativeCallbacks;
}

namespace haylen::plugins {

// Installs `haylen.native` and keeps the Lua functions of the native callbacks of the app, which it drops when the app stops so late native calls never reach Lua.
class NativePlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "native";
    }
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    [[nodiscard]] const std::shared_ptr<platform::NativeCallbacks>& getCallbacks() const noexcept {
        return callbacks;
    }

  private:
    std::shared_ptr<platform::NativeCallbacks> callbacks;
};

} // namespace haylen::plugins
