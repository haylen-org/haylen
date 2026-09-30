#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "2d/spatial/ScriptedIndex.hpp"
#include "haylen/2d/spatial/Neighbor.hpp"
#include "haylen/2d/spatial/RayHit.hpp"
#include "haylen/lua/ClassBuilder.hpp"

namespace haylen::spatial2d {

// Installs the `HashGrid`, `QuadTree`, `AabbTree` and `KdTree` classes of `haylen.spatial2d`. Each stores any Lua value, keyed by identity like table keys, and all share their query methods. The user value of a structure holds `ids`, which maps each stored value to its id, and `values`, which maps ids back.
class SpatialIndexLua final {
  public:
    static void install(lua_State* L);

    // Sets the constructors of the structures on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kQuadTreeFields{"maxEntries", "maxDepth"};

    static void resetTables(lua_State* L, int owner);
    [[nodiscard]] static std::optional<std::uint64_t> idOf(lua_State* L, int valueIndex);
    static void pushValues(lua_State* L, const std::vector<std::uint64_t>& ids);
    static void pushHits(lua_State* L, const std::vector<RayHit>& hits, float length);

    // Stores the value at index 2 under the id, after the structure accepted it.
    static void remember(lua_State* L, std::uint64_t id);

    // Stores the value at index 2 in the structure with `storeEntry(id)`, reusing its id or giving it the next one.
    template <typename Structure, typename Store> static void store(lua_State* L, ScriptedIndex<Structure>& self, Store&& storeEntry);

    static int newHashGrid(lua_State* L);
    static int newQuadTree(lua_State* L);
    static int newAabbTree(lua_State* L);
    static int newKdTree(lua_State* L);

    template <typename Structure> static int set(lua_State* L);
    static int setPoint(lua_State* L);
    template <typename Structure> static int remove(lua_State* L);
    template <typename Structure> static int has(lua_State* L);
    template <typename Structure> static int bounds(lua_State* L);
    static int point(lua_State* L);
    static int build(lua_State* L);
    static int isBuilt(lua_State* L);
    template <typename Structure> static int query(lua_State* L);
    template <typename Structure> static int queryCircle(lua_State* L);
    template <typename Structure> static int queryPoint(lua_State* L);
    template <typename Structure> static int raycast(lua_State* L);
    template <typename Structure> static int nearest(lua_State* L);
    template <typename Structure> static int nearestList(lua_State* L);
    template <typename Structure> static int pick(lua_State* L);
    template <typename Structure> static int clear(lua_State* L);
    template <typename Structure> static int size(lua_State* L);
    static int cellSize(lua_State* L);
    static int area(lua_State* L);
    static int nodeCount(lua_State* L);
    static int height(lua_State* L);
    static int margin(lua_State* L);

    // Adds the methods every structure shares to its class.
    template <typename Structure> static lua::ClassBuilder<ScriptedIndex<Structure>>& addQueries(lua::ClassBuilder<ScriptedIndex<Structure>>& builder);
};

} // namespace haylen::spatial2d
