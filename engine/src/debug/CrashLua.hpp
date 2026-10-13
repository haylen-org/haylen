#pragma once

struct lua_State;

namespace haylen::debug {

// Adds the breadcrumbs and the report of the last native crash to `haylen.debug`.
class CrashLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static int addBreadcrumb(lua_State* L);
    static int breadcrumbs(lua_State* L);
    static int lastCrash(lua_State* L);
};

} // namespace haylen::debug
