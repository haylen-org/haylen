#pragma once

struct lua_State;

namespace haylen::platform {

// Installs `haylen.system`, which tells what the device is, its theme, its battery, its power saving mode, its time zone and the languages the person prefers, and reads and writes the clipboard.
class SystemLua final {
  public:
    static void install(lua_State* L);

  private:
    static int info(lua_State* L);
    static int theme(lua_State* L);
    static int battery(lua_State* L);
    static int powerSaving(lua_State* L);
    static int timeZone(lua_State* L);
    static int languages(lua_State* L);
    static int clipboard(lua_State* L);
    static int setClipboard(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
