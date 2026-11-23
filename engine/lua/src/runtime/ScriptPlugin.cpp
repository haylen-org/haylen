#include "haylen/lua/ScriptPlugin.hpp"

#include <lua.hpp>

#include <limits>
#include <string>
#include <vector>

#include "debug/ExecutionBudget.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "modules/BuiltInModules.hpp"
#include "runtime/Autoloads.hpp"
#include "runtime/Environment.hpp"
#include "runtime/Jobs.hpp"
#include "runtime/LuaConfig.hpp"
#include "runtime/Warnings.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::lua {

ScriptPlugin::ScriptPlugin(const LuaConfig& settings) : config(std::make_unique<const LuaConfig>(settings)), warnings(std::make_unique<Warnings>()), autoloads(std::make_unique<Autoloads>()), jobs(std::make_unique<Jobs>()) {
    lua::modules::BuiltInModules::registerAll(modules);
}

// The engine lets go of its plugins once every service released the Lua values it held, so the Lua state closes here, after the modules, and the execution budget of development counts the memory that the state frees as it closes.
ScriptPlugin::~ScriptPlugin() {
    jobs.reset();
    autoloads.reset();
    modules.clear();
    runtime.reset();

    if (session != nullptr) {
        Development::of(*session).getExecutionBudget().detach();
    }
}

void ScriptPlugin::start(haylen::core::Engine& engine) {
    owner = &engine;
    session = engine.getDevelopmentSession();
    runtime = std::make_unique<varn::runtime::Runtime>(engine.getJobs().getScheduler(), std::vector<std::string>{"haylen", std::string(engine.getPackage().getName())}, 1);

    lua_State* L = runtime->luaState();
    Environment::install(engine, L);
    warnings->install(L);
    for (std::size_t index = 0; index < modules.size(); ++index) {
        install(modules[index]);
    }

    if (session != nullptr) {
        attachBudget(engine, L);
    }

    if (!engine.hasNativeAccess()) {
        Environment::withholdNativeAccess(L);
    }

    Environment::checkModules(engine);
}

void ScriptPlugin::install(Module& module) {
    module.install(*owner, runtime->luaState());
}

// The budget of development holds the hooks of Lua from before the code of the app runs until the state closed, so `debug.sethook` is refused, and it cancels the app through the engine once its code ran too long in one entry of the host or took too much memory.
void ScriptPlugin::attachBudget(haylen::core::Engine& engine, lua_State* L) {
    // clang-format off
    const auto cancel = [&engine](const haylen::core::AppError& error) {
        engine.cancel(error);
    };
    const auto stage = [&engine]() -> std::string_view {
        if (engine.getPhase() == haylen::core::Engine::Phase::Reload) {
            return "reload";
        }
        return engine.getProfiler().getScopeName();
    };
    // clang-format on
    Development::of(*session).getExecutionBudget().attach(L, {.time = config->executionBudget, .memory = config->memoryBudget}, cancel, stage);

    lua::debug::ExecutionBudget::guardMessageHandlers(L);
    lua::debug::ExecutionBudget::guardFinalizers(L);
    lua_getglobal(L, "debug");
    lua_pushcfunction(L, &refuseHook);
    lua_setfield(L, -2, "sethook");
    lua_pop(L, 1);
}

int ScriptPlugin::refuseHook(lua_State* L) {
    return luaL_error(L, "The function \"debug.sethook\" is not available in development, where the execution budget and the debugger of the engine hold the hooks of Lua.");
}

// The app stops its autoloads before the plugins stop, and the jobs and the modules let go of their Lua values here, while every plugin of the engine still runs.
void ScriptPlugin::stop(haylen::core::Engine& engine) {
    autoloads->stop(engine);
    jobs->stop();
    for (std::size_t index = modules.size(); index > 0; --index) {
        modules[index - 1].stop(engine);
    }
}

// The app stops while the error screen shows, so its autoloads hear no events, like its scenes.
void ScriptPlugin::event(haylen::core::Engine& engine, const haylen::platform::Event& event) {
    if (engine.getError() == nullptr) {
        autoloads->event(engine, event);
    }
}

void ScriptPlugin::fixedUpdate(haylen::core::Engine& engine, float stepSeconds) {
    autoloads->fixedUpdate(engine, stepSeconds);
}

void ScriptPlugin::update(haylen::core::Engine& engine, float deltaSeconds) {
    autoloads->update(engine, deltaSeconds);
    jobs->update();
}

void ScriptPlugin::render(haylen::core::Engine&) {
    autoloads->render();
}

void ScriptPlugin::renderUi(haylen::core::Engine&, const haylen::core::SceneView& view) {
    if (view.current) {
        autoloads->renderUi();
    }
}

bool ScriptPlugin::isAnimating() const {
    return jobs->isAnimating();
}

lua_State* ScriptPlugin::getState() const noexcept {
    return runtime->luaState();
}

varn::runtime::Runtime& ScriptPlugin::getRuntime() noexcept {
    return *runtime;
}

const LuaConfig& ScriptPlugin::getConfig() const noexcept {
    return *config;
}

Autoloads& ScriptPlugin::getAutoloads() noexcept {
    return *autoloads;
}

Jobs& ScriptPlugin::getJobs() noexcept {
    return *jobs;
}

} // namespace haylen::lua
