#include "haylen/lua/Application.hpp"

#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "lua/Autoloads.hpp"
#include "lua/Environment.hpp"
#include "plugins/CorePlugin.hpp"

namespace haylen::lua {

void Application::start(core::Engine& engine) {
    Autoloads& autoloads = engine.getPlugin<plugins::CorePlugin>().getAutoloads();
    for (const std::string& module : engine.getConfig().autoloads) {
        autoloads.add(engine.getLuaState(), Autoloads::getName(module), module);
    }

    Environment::runModule(engine.getLuaState(), engine.getPackage(), std::string(io::Path::kSourceDirectory) + "/main.lua");
}

void Application::stop(core::Engine& engine) {
    engine.getPlugin<plugins::CorePlugin>().getAutoloads().stop(engine);
}

} // namespace haylen::lua
