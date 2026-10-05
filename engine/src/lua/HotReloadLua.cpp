#include "lua/HotReloadLua.hpp"

#include <string>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "lua/ModuleGraph.hpp"
#include "plugins/HotReloadPlugin.hpp"

namespace haylen::lua {

int HotReloadLua::active(lua_State* L) {
    Stack::push(L, Runtime::getEngine(L).getPlugin<plugins::HotReloadPlugin>().isActive());
    return 1;
}

int HotReloadLua::mode(lua_State* L) {
    Stack::push(L, core::AppConfig::reloadName(Runtime::getEngine(L).getConfig().debug.reload));
    return 1;
}

// Returns `keep(key, create)`: the value the module kept under the key in its previous load, or the result of `create()`, which it keeps. Outside development every call creates.
int HotReloadLua::keep(lua_State* L) {
    const std::string key = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    ModuleGraph* graph = Runtime::getEngine(L).getPlugin<plugins::HotReloadPlugin>().getGraph();
    if (graph == nullptr) {
        lua_call(L, 0, 1);
        return 1;
    }
    const std::string path(graph->getLoading());
    if (path.empty()) {
        return luaL_error(L, "The function \"keep\" works only while a module loads.");
    }

    ModuleGraph::pushKept(L, path);
    lua_getfield(L, 3, key.c_str());
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);
    lua_pushvalue(L, 2);
    lua_call(L, 0, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, 3, key.c_str());
    return 1;
}

// Marks the module that loads, so that a change to its file restarts the app instead of reloading the module.
int HotReloadLua::restartOnChange(lua_State* L) {
    ModuleGraph* graph = Runtime::getEngine(L).getPlugin<plugins::HotReloadPlugin>().getGraph();
    if (graph == nullptr) {
        return 0;
    }
    ModuleGraph::Module* module = graph->find(graph->getLoading());
    if (module == nullptr) {
        return luaL_error(L, "The function \"restartOnChange\" works only while a module loads.");
    }
    module->restartOnChange = true;
    return 0;
}

int HotReloadLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"active", &active}, {"mode", &mode}, {"keep", &Binding::native<&keep>}, {"restartOnChange", &Binding::native<&restartOnChange>}, {nullptr, nullptr},
    };
    Binding::newModule(L, functions);
    return 1;
}

void HotReloadLua::install(lua_State* L) {
    Binding::preload(L, "haylen.hotReload", &open);
}

} // namespace haylen::lua
