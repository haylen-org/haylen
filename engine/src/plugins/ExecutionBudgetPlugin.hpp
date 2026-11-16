#pragma once

#include <string_view>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::plugins {

// Gives the Lua state of an app in development the execution budget of the development session, from before its code runs until the state closed, and cancels the app through the engine once its code ran longer than `debug.executionBudget` of `app.json` in one entry of the host. It also keeps `debug.sethook` from the app, since the budget and the debugger hold the hooks of Lua. Only an app in development has a budget, so an app that ships never hooks its Lua.
class ExecutionBudgetPlugin final : public Plugin {
  public:
    explicit ExecutionBudgetPlugin(platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}
    ~ExecutionBudgetPlugin() override;

    ExecutionBudgetPlugin(const ExecutionBudgetPlugin&) = delete;
    ExecutionBudgetPlugin& operator=(const ExecutionBudgetPlugin&) = delete;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "executionBudget";
    }
    void installLua(core::Engine& engine, lua_State* L) override;
    void stop(core::Engine& engine) override;

  private:
    static int refuseHook(lua_State* L);

    platform::DevelopmentSession* session;
};

} // namespace haylen::plugins
