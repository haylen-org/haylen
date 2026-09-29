#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::core {

// Installs haylen.timer, whose timers call Lua functions after a delay or at an interval, on scaled or unscaled time and with a process mode.
class TimerLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 4> kTimerFields{"owner", "processMode", "unscaled", "count"};

    static int start(lua_State* L, bool repeating);
    static int after(lua_State* L);
    static int every(lua_State* L);
    static int cancel(lua_State* L);
    static int pause(lua_State* L);
    static int active(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::core
