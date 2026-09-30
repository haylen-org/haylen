#pragma once

struct lua_State;

namespace haylen::platform {

// Installs haylen.system, which tells what the device is, its theme and its battery, and opens urls and vibrates the device.
class SystemLua final {
  public:
    static void install(lua_State* L);

  private:
    static int info(lua_State* L);
    static int theme(lua_State* L);
    static int battery(lua_State* L);
    static int openUrl(lua_State* L);
    static int vibrate(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
