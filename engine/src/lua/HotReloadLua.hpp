#pragma once

#include <lua.hpp>

namespace haylen::lua {

// The Lua module `haylen.hotReload`, which every app has: whether the app runs in development, how changed modules apply, values that a module keeps across its reloads, and modules that restart the app when they change. Apps that ship use the same calls, which then keep nothing and never reload.
class HotReloadLua final {
  public:
    static void install(lua_State* L);

  private:
    static int open(lua_State* L);
    static int active(lua_State* L);
    static int mode(lua_State* L);
    static int keep(lua_State* L);
    static int restartOnChange(lua_State* L);
};

} // namespace haylen::lua
