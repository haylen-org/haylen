#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "haylen/platform/Permission.hpp"

struct lua_State;

namespace haylen::core {
class Engine;
}

namespace haylen::lua {
class Promise;
}

namespace haylen::platform {

// Installs `haylen.system`, which tells what the device is, its theme, its battery, its power saving mode, its time zone and the languages the person prefers, opens urls, reads and writes the clipboard, vibrates the device, reads and asks for the permissions the person grants at run time and shows what the app plays in the now-playing controls of the system.
class SystemLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 8> kNowPlayingFields{"title", "artist", "album", "duration", "position", "playing", "artwork", "commands"};

    static int info(lua_State* L);
    static int theme(lua_State* L);
    static int battery(lua_State* L);
    static int powerSaving(lua_State* L);
    static int timeZone(lua_State* L);
    static int languages(lua_State* L);
    static int openUrl(lua_State* L);
    static int clipboard(lua_State* L);
    static int setClipboard(lua_State* L);
    static int vibrate(lua_State* L);
    static int permission(lua_State* L);
    static int requestPermission(lua_State* L);
    static int setNowPlaying(lua_State* L);
    static int clearNowPlaying(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
