#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "2d/spatial/ScriptedOverlapWorld.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<spatial2d::ScriptedOverlapWorld> {
    static constexpr const char* name = "haylen.OverlapWorld";
    using Storage = spatial2d::ScriptedOverlapWorld;
};

} // namespace haylen::lua

namespace haylen::spatial2d {

// Installs the `OverlapWorld` class of `haylen.spatial2d` and `spatial2d.newOverlapWorld`, whose bulk methods read placements from a float buffer and write contacts into one, so a step of any number of entities takes two calls.
class OverlapWorldLua final {
  public:
    static void install(lua_State* L);

    // Sets `newOverlapWorld` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kOptions{"kinds", "touching", "cellSize"};
    static constexpr std::array<std::string_view, 8> kBoxFields{"kind", "x", "y", "width", "height", "layers", "mask", "depth"};
    static constexpr std::size_t kPlacementSize = 6;
    static constexpr std::size_t kContactSize = 8;

    [[nodiscard]] static ScriptedOverlapWorld& check(lua_State* L);
    [[nodiscard]] static std::uint32_t readId(lua_State* L, int index);
    [[nodiscard]] static std::uint32_t readMask(lua_State* L, const char* field, std::uint32_t fallback);

    static int newOverlapWorld(lua_State* L);
    static int defineSet(lua_State* L);
    static int add(lua_State* L);
    static int place(lua_State* L);
    static int placeAll(lua_State* L);
    static int remove(lua_State* L);
    static int contacts(lua_State* L);
    static int rest(lua_State* L);
    static int resting(lua_State* L);
    static int step(lua_State* L);
    static int clear(lua_State* L);
    static int debugDraw(lua_State* L);
    static int entityCount(lua_State* L);
};

} // namespace haylen::spatial2d
