#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::platform {

// Installs `haylen.system`, which tells what the device is, its theme and its battery, opens urls, vibrates the device and shows what the app plays in the now-playing controls of the system.
class SystemLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 8> kNowPlayingFields{"title", "artist", "album", "duration", "position", "playing", "artwork", "commands"};

    static int info(lua_State* L);
    static int theme(lua_State* L);
    static int battery(lua_State* L);
    static int openUrl(lua_State* L);
    static int vibrate(lua_State* L);
    static int setNowPlaying(lua_State* L);
    static int clearNowPlaying(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
