#pragma once

struct lua_State;

namespace haylen::platform {

// Installs haylen.platform, which calls native methods, answers them from Lua and listens to native events.
class PlatformLua final {
  public:
    static void install(lua_State* L);

  private:
    static int call(lua_State* L);
    static int resolve(lua_State* L);
    static int emit(lua_State* L);
    static int pendingCalls(lua_State* L);
    static int on(lua_State* L);
    static int registerHandler(lua_State* L);
    static int hasHandler(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
