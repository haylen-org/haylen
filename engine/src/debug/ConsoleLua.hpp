#pragma once

#include <array>
#include <string_view>
#include <utility>

#include "haylen/debug/Console.hpp"

struct lua_State;

namespace haylen::debug {

// Adds the console functions to `haylen.debug`: commands that Lua registers with typed arguments, running a line, completions, the history, and the visibility and key of the console.
class ConsoleLua final {
  public:
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kCommandFields{"help", "args", "owner"};
    static constexpr std::array<std::pair<std::string_view, Console::Kind>, 5> kKinds{{{"integer", Console::Kind::Integer}, {"number", Console::Kind::Number}, {"string", Console::Kind::String}, {"boolean", Console::Kind::Boolean}, {"text", Console::Kind::Text}}};

    [[nodiscard]] static Console::Parameter readParameter(lua_State* L, int index);
    [[nodiscard]] static Console& getConsole(lua_State* L);

    static int addCommand(lua_State* L);
    static int removeCommand(lua_State* L);
    static int runCommand(lua_State* L);
    static int commands(lua_State* L);
    static int completeCommand(lua_State* L);
    static int commandHistory(lua_State* L);
    static int setConsoleVisible(lua_State* L);
    static int consoleVisible(lua_State* L);
    static int consoleEnabled(lua_State* L);
    static int setConsoleKey(lua_State* L);
    static int consoleKey(lua_State* L);
};

} // namespace haylen::debug
