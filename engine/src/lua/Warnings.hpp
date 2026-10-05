#pragma once

#include <lua.hpp>

#include <string>

namespace haylen::lua {

// Writes the warnings of Lua, such as an error in a `__gc` metamethod or a call of `warn`, to the log of the engine instead of the standard error stream, which phones and pages do not show. The control messages `@off` and `@on` of `warn` turn them off and on, and they start on.
class Warnings final {
  public:
    // Sends the warnings of the Lua state to this object, which outlives the state.
    void install(lua_State* L);

  private:
    static void receive(void* data, const char* message, int continued);

    std::string pending;
    bool enabled = true;
};

} // namespace haylen::lua
