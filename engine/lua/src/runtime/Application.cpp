#include "haylen/lua/Application.hpp"

#include <memory>
#include <string>

#include "debug/ExecutionBudget.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "plugins/DebuggerPlugin.hpp"
#include "plugins/ExecutionBudgetPlugin.hpp"
#include "runtime/Autoloads.hpp"
#include "runtime/Config.hpp"
#include "runtime/Environment.hpp"

namespace haylen::lua {

const std::array<std::string_view, 1> Application::kSections{Config::kSection};

std::span<const std::string_view> Application::getSections() const {
    return kSections;
}

void Application::configure(haylen::core::AppConfig& config) {
    (void)Config::fromConfig(config);
}

// The tools of development that follow the Lua state start after the script plugin made it, and the execution budget of development times every call of the host into the engine from its start on.
void Application::addPlugins(haylen::core::Engine& engine) {
    if (haylen::platform::DevelopmentSession* session = engine.getDevelopmentSession()) {
        engine.setEntryObserver(&Development::of(*session).getExecutionBudget());
    }

    engine.addPlugin(std::make_unique<ScriptPlugin>(Config::fromConfig(engine.getConfig())));
    engine.addPlugin(std::make_unique<lua::plugins::ExecutionBudgetPlugin>(engine.getDevelopmentSession()));
    engine.addPlugin(std::make_unique<lua::plugins::DebuggerPlugin>(engine.getDevelopmentSession()));
}

void Application::start(haylen::core::Engine& engine) {
    ScriptPlugin& script = engine.getPlugin<ScriptPlugin>();
    for (const std::string& module : script.getConfig().autoloads) {
        script.getAutoloads().add(script.getState(), Autoloads::getName(module), module);
    }

    Environment::runModule(script.getState(), engine.getPackage(), std::string(haylen::io::Path::kSourceDirectory) + "/main.lua");
}

void Application::stop(haylen::core::Engine& engine) {
    engine.getPlugin<ScriptPlugin>().getAutoloads().stop(engine);
}

} // namespace haylen::lua
