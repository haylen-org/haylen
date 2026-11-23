#pragma once

#include <string_view>

#include "haylen/core/ScopedConnection.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::lua::plugins {

// Gives the code that stops an app in development the whole execution budget of the development session, which the script plugin gives the Lua state of the app from before its code runs until the state closed. Only an app in development has a budget, so an app that ships never hooks its Lua.
class ExecutionBudgetPlugin final : public haylen::plugins::Plugin {
  public:
    explicit ExecutionBudgetPlugin(haylen::platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "executionBudget";
    }
    void start(haylen::core::Engine& engine) override;
    void stop(haylen::core::Engine& engine) override;

  private:
    haylen::platform::DevelopmentSession* session;
    haylen::core::ScopedConnection stopping;
};

} // namespace haylen::lua::plugins
