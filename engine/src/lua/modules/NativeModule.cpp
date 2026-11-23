#include "lua/modules/NativeModule.hpp"

#include <memory>
#include <stdexcept>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/platform/NativeLibraries.hpp"
#include "platform/native/NativeCallbacks.hpp"
#include "platform/native/NativeLua.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::lua::modules {

// The symbols of the linked libraries reach `ffi.C` by name, so the app need not export them.
void NativeModule::install(core::Engine& engine, lua_State* L) {
    varn::runtime::Runtime& runtime = engine.getPlugin<ScriptPlugin>().getRuntime();
    for (const platform::NativeLibraries::LinkedSymbol& symbol : platform::NativeLibraries::getLinkedSymbols()) {
        if (!runtime.addSymbol(symbol.name, symbol.address)) {
            throw std::runtime_error("The symbol \"" + symbol.name + "\" of the linked library \"" + symbol.library + "\" has another address in another linked library. Register each symbol with one address.");
        }
    }

    callbacks = std::make_shared<platform::NativeCallbacks>(L);
    platform::NativeLua::install(L);
}

void NativeModule::stop(core::Engine&) {
    callbacks.reset();
}

} // namespace haylen::lua::modules
