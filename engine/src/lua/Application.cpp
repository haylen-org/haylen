#include "haylen/lua/Application.hpp"

#include <memory>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "lua/Autoloads.hpp"
#include "lua/Environment.hpp"
#include "plugins/DebuggerPlugin.hpp"
#include "plugins/ExecutionBudgetPlugin.hpp"

namespace haylen::lua {

// The tools of development that follow the Lua state start after the script plugin made it.
void Application::addPlugins(core::Engine& engine) {
    engine.addPlugin(std::make_unique<ScriptPlugin>());
    engine.addPlugin(std::make_unique<plugins::ExecutionBudgetPlugin>(engine.getDevelopmentSession()));
    engine.addPlugin(std::make_unique<plugins::DebuggerPlugin>(engine.getDevelopmentSession()));
}

void Application::start(core::Engine& engine) {
    ScriptPlugin& script = engine.getPlugin<ScriptPlugin>();
    for (const std::string& module : engine.getConfig().autoloads) {
        script.getAutoloads().add(script.getState(), Autoloads::getName(module), module);
    }

    Environment::runModule(script.getState(), engine.getPackage(), std::string(io::Path::kSourceDirectory) + "/main.lua");
}

void Application::stop(core::Engine& engine) {
    engine.getPlugin<ScriptPlugin>().getAutoloads().stop(engine);
}

} // namespace haylen::lua
