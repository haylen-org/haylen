#pragma once

#include <lua.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "haylen/2d/spatial/Cell.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

// GridLua installs the metatable of this type, and the map generators of haylen.procedural2d push their maps with it.
template <> struct Type<spatial2d::CellGrid> {
    static constexpr const char* name = "haylen.CellGrid";
    using Storage = spatial2d::CellGrid;
};

} // namespace haylen::lua

namespace haylen::spatial2d {

// Installs the CellGrid and UnionFind classes of haylen.spatial2d and its grid algorithms: grid ray casts and traversals, Bresenham lines and circles, field of view, visibility polygons, flood fills and connected regions. Cells cross into Lua as {x = column, y = row} tables.
class GridLua final {
  public:
    static void install(lua_State* L);

    // Sets the grid functions on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kFillFields{"diagonal", "value"};
    static constexpr std::array<std::string_view, 2> kRegionFields{"diagonal", "background"};
    static constexpr std::size_t kMaxTraversedCells = 65536;

    [[nodiscard]] static Cell readCell(lua_State* L, int first);
    static void pushCells(lua_State* L, const std::vector<Cell>& cells);

    // Reads a cell size given as a number or a {width, height} pair.
    [[nodiscard]] static math::Vec2 readCellSize(lua_State* L, int index);

    // Reads a 1-based union-find element and returns its 0-based index.
    [[nodiscard]] static std::size_t readElement(lua_State* L, int index);

    static int newCellGrid(lua_State* L);
    static int gridGet(lua_State* L);
    static int gridSet(lua_State* L);
    static int gridFill(lua_State* L);
    static int gridContains(lua_State* L);
    static int gridWidth(lua_State* L);
    static int gridHeight(lua_State* L);

    static int newUnionFind(lua_State* L);
    static int unionAdd(lua_State* L);
    static int unionFind(lua_State* L);
    static int unionUnite(lua_State* L);
    static int unionConnected(lua_State* L);
    static int unionSetSize(lua_State* L);
    static int unionSize(lua_State* L);
    static int unionSetCount(lua_State* L);
    static int unionReset(lua_State* L);

    static int raycastGrid(lua_State* L);
    static int traverseGrid(lua_State* L);
    static int line(lua_State* L);
    static int circle(lua_State* L);
    static int fieldOfView(lua_State* L);
    static int visibilityPolygon(lua_State* L);
    static int floodFill(lua_State* L);
    static int components(lua_State* L);
};

} // namespace haylen::spatial2d
