#include "plugins/ExecutionBudgetPlugin.hpp"

#include "debug/ExecutionBudget.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/Profiler.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::plugins {

// The engine destroys its plugins once the Lua state closed, so the budget counts the memory that the state frees as it closes.
ExecutionBudgetPlugin::~ExecutionBudgetPlugin() {
    if (session != nullptr) {
        session->getExecutionBudget().detach();
    }
}

void ExecutionBudgetPlugin::installLua(core::Engine& engine, lua_State* L) {
    if (session == nullptr) {
        return;
    }

    // clang-format off
    const auto cancel = [&engine](const lua::Error& error) {
        engine.cancel(error);
    };
    const auto stage = [&engine]() -> std::string_view {
        if (engine.getPhase() == core::Engine::Phase::Reload) {
            return "reload";
        }
        return engine.getProfiler().getScopeName();
    };
    // clang-format on
    const core::AppConfig::Debug& debug = engine.getConfig().debug;
    session->getExecutionBudget().attach(L, {.time = debug.executionBudget, .memory = debug.memoryBudget}, cancel, stage);

    debug::ExecutionBudget::guardMessageHandlers(L);
    lua_getglobal(L, "debug");
    lua_pushcfunction(L, &refuseHook);
    lua_setfield(L, -2, "sethook");
    lua_pop(L, 1);
}

void ExecutionBudgetPlugin::stop(core::Engine&) {
    if (session != nullptr) {
        session->getExecutionBudget().finishApp();
    }
}

int ExecutionBudgetPlugin::refuseHook(lua_State* L) {
    return luaL_error(L, "The function \"debug.sethook\" is not available in development, where the execution budget and the debugger of the engine hold the hooks of Lua.");
}

} // namespace haylen::plugins
