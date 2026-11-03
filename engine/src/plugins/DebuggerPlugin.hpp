#pragma once

#include <string_view>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::plugins {

// Lets the Lua debugger of the development session follow the Lua state of the app, from before its code runs until the state closes, and hands the debugger the requests of its client at the start of every frame. Only an app in development has a debugger, so an app that ships never hooks its Lua.
class DebuggerPlugin final : public Plugin {
  public:
    explicit DebuggerPlugin(platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "debugger";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void stop(core::Engine& engine) override;

  private:
    platform::DevelopmentSession* session;
};

} // namespace haylen::plugins
