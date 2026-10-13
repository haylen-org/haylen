#pragma once

struct lua_State;

namespace haylen::platform {

// Installs `haylen.accessibility`, which reads the accessibility settings of the system.
class AccessibilityLua final {
  public:
    static void install(lua_State* L);

  private:
    static int settings(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
