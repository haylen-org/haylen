#include "haylen/lua/Binding.hpp"

#include <stdexcept>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Runtime.hpp"
#include "varn/runtime/Runtime.h"

namespace haylen::lua {

void Binding::preload(lua_State* L, const char* name, lua_CFunction opener) {
    if (!Runtime::getEngine(L).getScriptRuntime().addModule(name, opener)) {
        throw std::runtime_error("The Lua module \"" + std::string(name) + "\" cannot be added, because a module of Varn or of another plugin already has its name. Give the module another name.");
    }
}

void Binding::newModule(lua_State* L, const luaL_Reg* functions) {
    lua_newtable(L);
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua
