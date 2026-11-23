#include "plugins/DebuggerPlugin.hpp"

#include "debug/Debugger.hpp"
#include "development/Development.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::plugins {

// The script plugin starts before the debugger, so the debugger follows the Lua state from before the code of the app runs.
void DebuggerPlugin::start(core::Engine& engine) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().attach(engine.getPlugin<lua::ScriptPlugin>().getState());
    }
}

void DebuggerPlugin::beginFrame(core::Engine& engine, float) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().update(engine.getPlugin<lua::ScriptPlugin>().getState());
    }
}

void DebuggerPlugin::stop(core::Engine&) {
    if (session != nullptr) {
        lua::Development::of(*session).getDebugger().detach();
    }
}

} // namespace haylen::plugins
