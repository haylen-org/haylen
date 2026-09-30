#pragma once

#include <lua.hpp>

namespace haylen::math {

// Installs the `Spring` class of `haylen.math` with its constructor and `smoothDamp`.
class SpringLua final {
  public:
    static void install(lua_State* L);

    // Sets `spring` and `smoothDamp` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int newSpring(lua_State* L);
    static int update(lua_State* L);
    static int smoothDamp(lua_State* L);
};

} // namespace haylen::math
