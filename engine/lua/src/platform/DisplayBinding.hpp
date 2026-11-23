#pragma once

#include "haylen/platform/Display.hpp"

struct lua_State;

namespace haylen::lua::platform {

// Installs `haylen.display`, which lists the displays of the device, tells which one shows the window, asks browsers to show every screen and opens outputs of the app on other displays.
class DisplayBinding final {
  public:
    static void install(lua_State* L);

  private:
    static void pushDisplay(lua_State* L, const haylen::platform::Display& display);

    static int list(lua_State* L);
    static int current(lua_State* L);
    static int unlisted(lua_State* L);
    static int requestEveryScreen(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::platform
