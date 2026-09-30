#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>

#include "haylen/2d/procedural/CellularAutomaton.hpp"
#include "haylen/2d/procedural/DrunkardWalk.hpp"
#include "haylen/2d/procedural/Dungeon.hpp"
#include "haylen/2d/procedural/Maze.hpp"
#include "haylen/2d/procedural/WaveFunctionCollapse.hpp"

namespace haylen::procedural2d {

// Adds the map generators of `haylen.procedural2d`, their asynchronous versions, the `Maze` and `WaveFunctionCollapseRules` classes and autotiling. Maps cross into Lua as `CellGrid` objects of `haylen.spatial2d`, and tile numbers keep their values.
class MapGeneratorsLua final {
  public:
    static void install(lua_State* L);

    // Sets the generator functions on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 9> kCaveFields{"width", "height", "fillChance", "steps", "birthLimit", "survivalLimit", "solidBorder", "seed", "random"};
    static constexpr std::array<std::string_view, 7> kWalkFields{"width", "height", "coverage", "walkers", "maxSteps", "seed", "random"};
    static constexpr std::array<std::string_view, 11> kDungeonFields{"method", "width", "height", "minimumRoomSize", "maximumRoomSize", "minimumLeafSize", "maximumRooms", "roomAttempts", "padding", "seed", "random"};
    static constexpr std::array<std::string_view, 5> kMazeFields{"width", "height", "algorithm", "seed", "random"};
    static constexpr std::array<std::string_view, 12> kCollapseFields{"tiles", "weights", "allow", "sample", "periodicSample", "width", "height", "periodic", "attempts", "fixed", "seed", "random"};
    static constexpr std::array<std::string_view, 5> kRulesFields{"tiles", "weights", "allow", "sample", "periodicSample"};
    static constexpr std::array<std::string_view, 3> kStepFields{"birthLimit", "survivalLimit", "solidBorder"};

    [[nodiscard]] static CellularAutomaton::Options readCaves(lua_State* L, int index);
    [[nodiscard]] static DrunkardWalk::Options readWalk(lua_State* L, int index);
    [[nodiscard]] static Dungeon::Options readDungeon(lua_State* L, int index);
    [[nodiscard]] static WaveFunctionCollapse::Rules readRules(lua_State* L, int index);
    [[nodiscard]] static WaveFunctionCollapse::Options readCollapse(lua_State* L, int index);
    static void pushDungeon(lua_State* L, Dungeon::Result result);
    static void pushCollapse(lua_State* L, std::optional<spatial2d::CellGrid> tiles);

    static int cellularAutomaton(lua_State* L);
    static int cellularAutomatonAsync(lua_State* L);
    static int cellularAutomatonStep(lua_State* L);
    static int drunkardWalk(lua_State* L);
    static int drunkardWalkAsync(lua_State* L);
    static int dungeon(lua_State* L);
    static int dungeonAsync(lua_State* L);
    static int maze(lua_State* L);
    static int mazeAsync(lua_State* L);
    static int newMaze(lua_State* L);
    static int waveFunctionCollapse(lua_State* L);
    static int waveFunctionCollapseAsync(lua_State* L);

    static int newRules(lua_State* L);
    static int rulesAllowed(lua_State* L);
    static int rulesWeight(lua_State* L);
    static int rulesTileCount(lua_State* L);
    static int rulesValid(lua_State* L);

    static int mazeOpenings(lua_State* L);
    static int mazeOpen(lua_State* L);
    static int mazeToGrid(lua_State* L);
    static int mazeWidth(lua_State* L);
    static int mazeHeight(lua_State* L);
    static int mazePassageCount(lua_State* L);

    static int mask4(lua_State* L);
    static int mask8(lua_State* L);
    static int blobIndex(lua_State* L);
    static int autotile4(lua_State* L);
    static int autotile8(lua_State* L);
    static int autotileWang(lua_State* L);
};

} // namespace haylen::procedural2d
