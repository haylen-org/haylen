#include "haylen/lua/Application.hpp"

#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Runtime.hpp"
#include "lua/Autoloads.hpp"
#include "plugins/CorePlugin.hpp"

namespace haylen::lua {

void Application::start(core::Engine& engine) {
    Autoloads& autoloads = engine.getPlugin<plugins::CorePlugin>().getAutoloads();
    for (const std::string& module : engine.getConfig().autoloads) {
        autoloads.add(engine.getLuaState(), Autoloads::getName(module), module);
    }

    const std::string main = std::string(io::Path::kSourceDirectory) + "/main.lua";
    Runtime::runChunk(engine.getLuaState(), engine.getPackage().readText(main), "@" + main);
}

void Application::stop(core::Engine& engine) {
    engine.getPlugin<plugins::CorePlugin>().getAutoloads().stop(engine);
}

} // namespace haylen::lua
