#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "3d/spatial/ScriptedIndex.hpp"
#include "haylen/lua/ClassBuilder.hpp"

namespace haylen::lua::spatial3d {

// Installs the `HashGrid3D`, `Octree`, `AabbTree3D`, `Bvh` and `KdTree3D` classes of `haylen.spatial3d`. Each stores any Lua value, keyed by identity like table keys, and all share their query methods. The user value of a structure holds `ids`, which maps each stored value to its id, and `values`, which maps ids back.
class SpatialIndexBinding final {
  public:
    static void install(lua_State* L);

    // Sets the constructors of the structures on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kOctreeFields{"maxEntries", "maxDepth", "looseness"};

    // Each entry of a batch of boxes takes six numbers, its smallest corner and then its largest, and each entry of a batch of spheres four, its center and its radius.
    static constexpr std::size_t kBoxFloats = 6;
    static constexpr std::size_t kSphereFloats = 4;

    static void resetTables(lua_State* L, int owner);
    [[nodiscard]] static std::optional<std::uint64_t> idOf(lua_State* L, int valueIndex);

    // Remembers the value at the stack index under the id, after the structure accepted it.
    static void remember(lua_State* L, int valueIndex, std::uint64_t id);

    // Forgets the values of the ids from `first` up to `end`, which a refused batch gave out.
    static void forgetFrom(lua_State* L, std::uint64_t first, std::uint64_t end);

    // Stores the value at index 2 in the structure with `storeEntry(id)`, reusing its id or giving it the next one.
    template <typename Structure, typename Store> static void store(lua_State* L, ScriptedIndex<Structure>& self, Store&& storeEntry);

    // Pushes the values of the ids as a list, into the table at `results` when it is given, whose later items it clears, or into a new table.
    static void pushValues(lua_State* L, const std::vector<std::uint64_t>& ids, int results);
    static void pushHits(lua_State* L, const std::vector<haylen::spatial3d::RayHit>& hits);

    // Reads the list of values at index 2 and the float buffer at index 3 into the ids and the bounds of the structure, `floats` numbers for each value, and stores them with `apply()`.
    template <typename Structure, typename Apply> static void storeBatch(lua_State* L, ScriptedIndex<Structure>& self, std::size_t floats, Apply&& apply);

    static int newHashGrid(lua_State* L);
    static int newOctree(lua_State* L);
    static int newAabbTree(lua_State* L);
    static int newBvh(lua_State* L);
    static int newKdTree(lua_State* L);

    template <typename Structure> static int set(lua_State* L);
    template <typename Structure> static int setAll(lua_State* L);
    static int setPoint(lua_State* L);
    static int setAllPoints(lua_State* L);
    template <typename Structure> static int remove(lua_State* L);
    template <typename Structure> static int has(lua_State* L);
    template <typename Structure> static int bounds(lua_State* L);
    static int sphere(lua_State* L);
    template <typename Structure> static int build(lua_State* L);
    template <typename Structure> static int isBuilt(lua_State* L);
    static int refit(lua_State* L);
    template <typename Structure> static int query(lua_State* L);
    template <typename Structure> static int querySphere(lua_State* L);
    template <typename Structure> static int queryPoint(lua_State* L);
    template <typename Structure> static int queryFrustum(lua_State* L);
    template <typename Structure> static int raycast(lua_State* L);
    template <typename Structure> static int nearest(lua_State* L);
    template <typename Structure> static int nearestList(lua_State* L);
    template <typename Structure> static int clear(lua_State* L);
    template <typename Structure> static int size(lua_State* L);
    static int cellSize(lua_State* L);
    static int region(lua_State* L);
    static int octreeNodeCount(lua_State* L);
    static int height(lua_State* L);
    static int margin(lua_State* L);
    static int bvhNodeCount(lua_State* L);
    static int depth(lua_State* L);

    // Adds the methods every structure shares to its class.
    template <typename Structure> static lua::ClassBuilder<ScriptedIndex<Structure>>& addQueries(lua::ClassBuilder<ScriptedIndex<Structure>>& builder);
};

} // namespace haylen::lua::spatial3d
