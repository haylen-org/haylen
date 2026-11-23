#pragma once

#include <memory>
#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::platform {
class NativeCallbacks;
}

namespace haylen::lua::modules {

// Installs `haylen.native`, gives `ffi.C` the symbols of the libraries linked into the app and keeps the Lua functions of the native callbacks of the app, which it drops when the app stops so late native calls never reach Lua.
class NativeModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.native";
    }
    void install(core::Engine& engine, lua_State* L) override;
    void stop(core::Engine& engine) override;

    [[nodiscard]] const std::shared_ptr<platform::NativeCallbacks>& getCallbacks() const noexcept {
        return callbacks;
    }

  private:
    std::shared_ptr<platform::NativeCallbacks> callbacks;
};

} // namespace haylen::lua::modules
