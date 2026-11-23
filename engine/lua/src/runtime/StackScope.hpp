#pragma once

#include <lua.hpp>

namespace haylen::lua {

// Restores the top of the Lua stack when it goes out of scope, also when an exception leaves the scope early.
class StackScope final {
  public:
    explicit StackScope(lua_State* L) noexcept : state(L), top(lua_gettop(L)) {}
    ~StackScope() {
        lua_settop(state, top);
    }

    StackScope(const StackScope&) = delete;
    StackScope& operator=(const StackScope&) = delete;

  private:
    lua_State* state;
    int top;
};

} // namespace haylen::lua
