#pragma once

#include <memory>

#include "haylen/core/Json.hpp"
#include "haylen/platform/DevelopmentSession.hpp"

namespace haylen::debug {
class Debugger;
class ExecutionBudget;
} // namespace haylen::debug

namespace haylen::lua {

// The part of a development session that Lua brings: the debugger of Lua, whose client reaches it through the development server, and the execution budget of the code of the app. The first app that runs Lua in the session makes it, and the session keeps it, so the breakpoints and the client outlive every app that restarts under it.
class Development final : public platform::DevelopmentSession::Extension {
  public:
    explicit Development(platform::DevelopmentSession& owner);
    ~Development() override;

    Development(const Development&) = delete;
    Development& operator=(const Development&) = delete;

    // The development of Lua of the session, which the call makes when the session has none yet.
    [[nodiscard]] static Development& of(platform::DevelopmentSession& session);

    [[nodiscard]] bool isHalted() const noexcept override;
    void receive(core::Json message) override;
    void disconnect() override;

    [[nodiscard]] debug::Debugger& getDebugger() const noexcept {
        return *debugger;
    }

    // The time that the code of the app may run in each call of the host into the engine and the memory of its Lua state.
    [[nodiscard]] debug::ExecutionBudget& getExecutionBudget() const noexcept {
        return *executionBudget;
    }

  private:
    std::unique_ptr<debug::ExecutionBudget> executionBudget;
    std::unique_ptr<debug::Debugger> debugger;
};

} // namespace haylen::lua
