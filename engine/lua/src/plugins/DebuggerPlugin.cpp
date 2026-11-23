#include "plugins/DebuggerPlugin.hpp"

#include "debug/Debugger.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/platform/DevelopmentSession.hpp"

namespace haylen::lua::plugins {

// The script plugin starts before the debugger, so the debugger follows the Lua state from before the code of the app runs.
void DebuggerPlugin::start(haylen::core::Engine& engine) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().attach(engine.getPlugin<lua::ScriptPlugin>().getState());
    }
}

void DebuggerPlugin::beginFrame(haylen::core::Engine& engine, float) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().update(engine.getPlugin<lua::ScriptPlugin>().getState());
    }
}

void DebuggerPlugin::stop(haylen::core::Engine&) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().detach();
    }
}

} // namespace haylen::lua::plugins
