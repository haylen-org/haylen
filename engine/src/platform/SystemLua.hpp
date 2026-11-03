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

// Installs `haylen.system`, which tells what the device is, its theme and its battery, opens urls, reads and writes the clipboard, vibrates the device, reads and asks for the permissions the person grants at run time and shows what the app plays in the now-playing controls of the system.
class SystemLua final {
  public:
    static void install(lua_State* L);

  private:
    // The sentences of the refused calls the engine logged, once each.
    static std::unordered_set<std::string> loggedRefusals;

    static constexpr std::array<std::string_view, 8> kNowPlayingFields{"title", "artist", "album", "duration", "position", "playing", "artwork", "commands"};

    static int info(lua_State* L);
    static int theme(lua_State* L);
    static int battery(lua_State* L);
    static int openUrl(lua_State* L);
    static int clipboard(lua_State* L);
    static int setClipboard(lua_State* L);
    static int vibrate(lua_State* L);
    static int permission(lua_State* L);
    static int requestPermission(lua_State* L);
    static void settlePermission(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer);
    static void settle(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer, std::optional<std::string> value);
    static void reject(core::Engine& engine, const lua::Promise& promise, const Permission::Answer& answer);
    static int setNowPlaying(lua_State* L);
    static int clearNowPlaying(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
