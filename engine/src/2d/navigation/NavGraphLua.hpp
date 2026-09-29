#pragma once

#include <lua.hpp>

#include <cstdint>
#include <span>

namespace haylen::navigation2d {

class Graph;

// Installs the NavGraph class of haylen.navigation2d, a waypoint graph whose points Lua names with integer ids.
class NavGraphLua final {
  public:
    static void install(lua_State* L);

    // Sets newGraph on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    [[nodiscard]] static Graph& check(lua_State* L);
    [[nodiscard]] static std::int64_t readId(lua_State* L, int index);
    static void pushIds(lua_State* L, std::span<const std::int64_t> ids);

    static int newGraph(lua_State* L);
    static int addPoint(lua_State* L);
    static int removePoint(lua_State* L);
    static int hasPoint(lua_State* L);
    static int getPosition(lua_State* L);
    static int setPosition(lua_State* L);
    static int getWeight(lua_State* L);
    static int setWeight(lua_State* L);
    static int isEnabled(lua_State* L);
    static int setEnabled(lua_State* L);
    static int connect(lua_State* L);
    static int disconnect(lua_State* L);
    static int isConnected(lua_State* L);
    static int neighbors(lua_State* L);
    static int points(lua_State* L);
    static int closest(lua_State* L);
    static int findPath(lua_State* L);
    static int distances(lua_State* L);
    static int clear(lua_State* L);
    static int size(lua_State* L);
};

} // namespace haylen::navigation2d
