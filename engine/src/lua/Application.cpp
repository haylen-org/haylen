#include "haylen/lua/Application.hpp"

#include <memory>
#include <string>

#include "debug/ExecutionBudget.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "lua/Autoloads.hpp"
#include "lua/Environment.hpp"
#include "plugins/DebuggerPlugin.hpp"
#include "plugins/ExecutionBudgetPlugin.hpp"
#include "runtime/LuaConfig.hpp"

namespace haylen::lua {

const std::array<std::string_view, 1> Application::kSections{LuaConfig::kSection};

std::span<const std::string_view> Application::getSections() const {
    return kSections;
}

void Application::configure(core::AppConfig& config) {
    (void)LuaConfig::fromConfig(config);
}

// The tools of development that follow the Lua state start after the script plugin made it, and the execution budget of development times every call of the host into the engine from its start on.
void Application::addPlugins(core::Engine& engine) {
    if (platform::DevelopmentSession* session = engine.getDevelopmentSession()) {
        engine.setEntryObserver(&Development::of(*session).getExecutionBudget());
    }

    engine.addPlugin(std::make_unique<ScriptPlugin>(LuaConfig::fromConfig(engine.getConfig())));
    engine.addPlugin(std::make_unique<plugins::ExecutionBudgetPlugin>(engine.getDevelopmentSession()));
    engine.addPlugin(std::make_unique<plugins::DebuggerPlugin>(engine.getDevelopmentSession()));
}

void Application::start(core::Engine& engine) {
    ScriptPlugin& script = engine.getPlugin<ScriptPlugin>();
    for (const std::string& module : script.getConfig().autoloads) {
        script.getAutoloads().add(script.getState(), Autoloads::getName(module), module);
    }

    Environment::runModule(script.getState(), engine.getPackage(), std::string(io::Path::kSourceDirectory) + "/main.lua");
}

void Application::stop(core::Engine& engine) {
    engine.getPlugin<ScriptPlugin>().getAutoloads().stop(engine);
}

} // namespace haylen::lua
