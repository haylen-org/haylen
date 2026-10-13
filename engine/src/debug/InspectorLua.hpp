#pragma once

struct lua_State;

namespace haylen::debug {

// Adds the inspector to `haylen.debug`: the values that the app exposes, the listing of a path and the visibility of the inspector window.
class InspectorLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static int watch(lua_State* L);
    static int unwatch(lua_State* L);
    static int inspect(lua_State* L);
    static int setInspectorVisible(lua_State* L);
    static int inspectorVisible(lua_State* L);
};

} // namespace haylen::debug
