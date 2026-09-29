#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/2d/navigation/DijkstraMap.hpp"
#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"
#include "haylen/2d/navigation/HierarchicalPathfinder.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::lua {
class Promise;
class Reference;
} // namespace haylen::lua

namespace haylen::navigation2d {

// Installs the NavGrid, DijkstraMap, FlowField and HierarchicalPathfinder classes of haylen.navigation2d. Cells cross into Lua as {x = column, y = row} tables, and maps, fields and hierarchies keep their grid alive in their user value.
class NavGridLua final {
  public:
    static void install(lua_State* L);

    // Sets newGrid on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kGridFields{"topology", "staggerX", "staggerEven"};
    static constexpr std::array<std::string_view, 5> kPathFields{"diagonal", "smooth", "heuristic", "weight", "jumpPoint"};
    static constexpr std::array<std::string_view, 1> kMapFields{"diagonal"};
    static constexpr std::array<std::string_view, 2> kHierarchyFields{"clusterSize", "diagonal"};

    [[nodiscard]] static Grid& check(lua_State* L);
    [[nodiscard]] static Grid::Cell readCell(lua_State* L, int first);
    [[nodiscard]] static GridSearch::Options readPathOptions(lua_State* L, int index);
    [[nodiscard]] static bool readDiagonal(lua_State* L, int index);
    [[nodiscard]] static HierarchicalPathfinder::Options readHierarchyOptions(lua_State* L, int index);

    // Reads a list of {x, y, value} cells, where the value defaults to 0.
    [[nodiscard]] static std::vector<DijkstraMap::Source> readSources(lua_State* L, int index);
    static void pushPath(lua_State* L, std::span<const Grid::Cell> path);

    // Pushes a new object of type T that keeps the grid at index alive in its user value.
    template <typename T, typename... Args> static T& pushOwned(lua_State* L, int gridIndex, Args&&... args);

    // Fetches the grid of the map, field or hierarchy at index 1 from its user value, leaving it on the stack.
    [[nodiscard]] static Grid& ownerGrid(lua_State* L);

    // Returns the copy of the grid at index 1 that background work reads, made once and shared until the grid changes.
    [[nodiscard]] static std::shared_ptr<const Grid> snapshotOf(lua_State* L);

    // Settles the promise with the result of a background job as an object that keeps the grid of owner alive. The promise pushes it once for every coroutine that awaits it, and each one gets its own copy.
    template <typename T> static void settleOwned(const lua::Promise& promise, const std::shared_ptr<lua::Reference>& owner, core::JobSystem::Result<T> result);

    static int newGrid(lua_State* L);
    static int getWidth(lua_State* L);
    static int getHeight(lua_State* L);
    static int getTopology(lua_State* L);
    static int isStaggerX(lua_State* L);
    static int isStaggerEven(lua_State* L);
    static int hasUniformCost(lua_State* L);
    static int getExpandedCount(lua_State* L);
    static int contains(lua_State* L);
    static int setWalkable(lua_State* L);
    static int isWalkable(lua_State* L);
    static int setCost(lua_State* L);
    static int getCost(lua_State* L);
    static int estimate(lua_State* L);
    static int findPath(lua_State* L);
    static int findPathAsync(lua_State* L);
    static int lineOfSight(lua_State* L);
    static int smoothPath(lua_State* L);
    static int raycast(lua_State* L);
    static int dijkstraMap(lua_State* L);
    static int dijkstraMapAsync(lua_State* L);
    static int flowField(lua_State* L);
    static int flowFieldAsync(lua_State* L);
    static int hierarchicalPathfinder(lua_State* L);
    static int hierarchicalPathfinderAsync(lua_State* L);

    static int mapValue(lua_State* L);
    static int mapNext(lua_State* L);
    static int mapFlee(lua_State* L);
    static int mapWidth(lua_State* L);
    static int mapHeight(lua_State* L);
    static int mapValues(lua_State* L);

    static int fieldNext(lua_State* L);
    static int fieldDirection(lua_State* L);
    static int fieldDistance(lua_State* L);
    static int fieldWidth(lua_State* L);
    static int fieldHeight(lua_State* L);

    static int hierarchyFindPath(lua_State* L);
    static int hierarchyUpdate(lua_State* L);
    static int hierarchyRebuild(lua_State* L);
    static int hierarchyNodeCount(lua_State* L);
    static int hierarchyClusterSize(lua_State* L);
    static int hierarchyDiagonal(lua_State* L);
};

} // namespace haylen::navigation2d
