#pragma once

#include <array>
#include <string_view>

#include "haylen/debug/Console.hpp"

struct lua_State;

namespace haylen::debug {

// Adds the console functions to `haylen.debug`: commands that Lua registers with typed arguments, running a line, listing the commands and whether lines from outside the app run.
class ConsoleLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kCommandFields{"help", "args", "owner"};

    [[nodiscard]] static Console::Parameter readParameter(lua_State* L, int index);
    [[nodiscard]] static Console& getConsole(lua_State* L);

    static int addCommand(lua_State* L);
    static int removeCommand(lua_State* L);
    static int runCommand(lua_State* L);
    static int commands(lua_State* L);
    static int consoleEnabled(lua_State* L);
};

} // namespace haylen::debug
