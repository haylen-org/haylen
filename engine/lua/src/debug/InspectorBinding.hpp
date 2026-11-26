#pragma once

struct lua_State;

namespace haylen::lua::debug {

// Adds the inspector to `haylen.debug`: the values that the app exposes, the listing of a path, the visibility and the key of the inspector window, and the pick on the screen with its selection.
class InspectorBinding final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static int watch(lua_State* L);
    static int unwatch(lua_State* L);
    static int inspect(lua_State* L);
    static int setInspectorVisible(lua_State* L);
    static int inspectorVisible(lua_State* L);
    static int setInspectorKey(lua_State* L);
    static int inspectorKey(lua_State* L);
    static int setPicking(lua_State* L);
    static int picking(lua_State* L);
    static int selected(lua_State* L);
};

} // namespace haylen::lua::debug
