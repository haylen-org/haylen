#include "plugins/DebuggerPlugin.hpp"

#include "debug/Debugger.hpp"
#include "haylen/core/Engine.hpp"
#include "platform/DevelopmentSession.hpp"

namespace haylen::plugins {

void DebuggerPlugin::installLua(core::Engine&, lua_State* L) {
    if (session != nullptr) {
        session->getDebugger().attach(L);
    }
}

void DebuggerPlugin::beginFrame(core::Engine& engine, float) {
    if (session != nullptr) {
        session->getDebugger().update(engine.getLuaState());
    }
}

void DebuggerPlugin::stop(core::Engine&) {
    if (session != nullptr) {
        session->getDebugger().detach();
    }
}

} // namespace haylen::plugins
