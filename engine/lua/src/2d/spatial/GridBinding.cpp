#include "2d/spatial/GridBinding.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/2d/spatial/Bresenham.hpp"
#include "haylen/2d/spatial/CellGrid.hpp"
#include "haylen/2d/spatial/ConnectedComponents.hpp"
#include "haylen/2d/spatial/FieldOfView.hpp"
#include "haylen/2d/spatial/FloodFill.hpp"
#include "haylen/2d/spatial/GridRay.hpp"
#include "haylen/2d/spatial/UnionFind.hpp"
#include "haylen/2d/spatial/VisibilityPolygon.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Segment.hpp"
#include "math/RaycastBinding.hpp"

namespace haylen::lua {

template <> struct Type<haylen::spatial2d::UnionFind> {
    static constexpr const char* name = "haylen.UnionFind";
    using Storage = haylen::spatial2d::UnionFind;
};

} // namespace haylen::lua

namespace haylen::lua::spatial2d {

haylen::spatial2d::Cell GridBinding::readCell(lua_State* L, int first) {
    return {lua::Stack::read<int>(L, first), lua::Stack::read<int>(L, first + 1)};
}

void GridBinding::pushCells(lua_State* L, const std::vector<haylen::spatial2d::Cell>& cells) {
    lua_createtable(L, static_cast<int>(cells.size()), 0);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        lua_createtable(L, 0, 2);
        lua_pushinteger(L, cells[index].x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, cells[index].y);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

std::size_t GridBinding::readElement(lua_State* L, int index, const haylen::spatial2d::UnionFind& sets) {
    const auto element = lua::Stack::read<lua_Integer>(L, index);
    luaL_argcheck(L, element >= 1, index, "union-find elements count from 1");
    if (static_cast<std::size_t>(element) > sets.size()) {
        luaL_error(L, "Element %I is outside the union-find of %I elements.", element, static_cast<lua_Integer>(sets.size()));
    }

    return static_cast<std::size_t>(element - 1);
}

int GridBinding::newCellGrid(lua_State* L) {
    const auto value = lua_isnoneornil(L, 3) ? std::int32_t{0} : lua::Stack::read<std::int32_t>(L, 3);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), value);
    return 1;
}

int GridBinding::gridGet(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).get(readCell(L, 2)));
    return 1;
}

int GridBinding::gridSet(lua_State* L) {
    lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).set(readCell(L, 2), lua::Stack::read<std::int32_t>(L, 4));
    return 0;
}

int GridBinding::gridFill(lua_State* L) {
    lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).fill(lua::Stack::read<std::int32_t>(L, 2));
    return 0;
}

int GridBinding::gridContains(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).contains(readCell(L, 2)));
    return 1;
}

int GridBinding::gridWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).getWidth());
    return 1;
}

int GridBinding::gridHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1).getHeight());
    return 1;
}

int GridBinding::newUnionFind(lua_State* L) {
    lua::Userdata::emplace<haylen::spatial2d::UnionFind>(L, lua_isnoneornil(L, 1) ? std::size_t{0} : lua::Stack::read<std::size_t>(L, 1));
    return 1;
}

int GridBinding::unionAdd(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1).add() + 1);
    return 1;
}

int GridBinding::unionFind(lua_State* L) {
    haylen::spatial2d::UnionFind& sets = lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1);
    lua::Stack::push(L, sets.find(readElement(L, 2, sets)) + 1);
    return 1;
}

int GridBinding::unionUnite(lua_State* L) {
    haylen::spatial2d::UnionFind& sets = lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1);
    const std::size_t first = readElement(L, 2, sets);
    lua::Stack::push(L, sets.unite(first, readElement(L, 3, sets)));
    return 1;
}

int GridBinding::unionConnected(lua_State* L) {
    haylen::spatial2d::UnionFind& sets = lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1);
    const std::size_t first = readElement(L, 2, sets);
    lua::Stack::push(L, sets.isConnected(first, readElement(L, 3, sets)));
    return 1;
}

int GridBinding::unionSetSize(lua_State* L) {
    haylen::spatial2d::UnionFind& sets = lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1);
    lua::Stack::push(L, sets.getSetSize(readElement(L, 2, sets)));
    return 1;
}

int GridBinding::unionSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1).size());
    return 1;
}

int GridBinding::unionSetCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1).getSetCount());
    return 1;
}

int GridBinding::unionReset(lua_State* L) {
    lua::Userdata::check<haylen::spatial2d::UnionFind>(L, 1).reset(lua::Stack::read<std::size_t>(L, 2));
    return 0;
}

haylen::math::Vec2 GridBinding::readCellSize(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        const auto side = lua::Stack::read<float>(L, index);
        return {side, side};
    }

    return lua::Stack::read<haylen::math::Vec2>(L, index);
}

// Casts over the solid cells of a grid with `raycastGrid(grid, from, to, cellSize)`, where the cell size is a number or a `{width, height}` pair.
int GridBinding::raycastGrid(lua_State* L) {
    const haylen::spatial2d::CellGrid& grid = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    const haylen::math::Ray ray = haylen::math::Ray::between(lua::Stack::read<haylen::math::Vec2>(L, 2), lua::Stack::read<haylen::math::Vec2>(L, 3));
    const std::optional<haylen::spatial2d::GridRay::Hit> hit = haylen::spatial2d::GridRay::cast(ray, readCellSize(L, 4), grid);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }

    lua::math::RaycastBinding::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, hit->cell.x);
    lua_setfield(L, -2, "column");
    lua_pushinteger(L, hit->cell.y);
    lua_setfield(L, -2, "row");
    return 1;
}

// Lists the cells that the ray from one point to another crosses with `traverseGrid(from, to, cellSize)`, in order, each as `{x, y, distance}` with the distance where the ray enters it.
int GridBinding::traverseGrid(lua_State* L) {
    const haylen::math::Ray ray = haylen::math::Ray::between(lua::Stack::read<haylen::math::Vec2>(L, 1), lua::Stack::read<haylen::math::Vec2>(L, 2));
    std::vector<std::pair<haylen::spatial2d::Cell, float>> crossed;
    // clang-format off
    haylen::spatial2d::GridRay::traverse(ray, readCellSize(L, 3), [&crossed](haylen::spatial2d::Cell cell, float distance, haylen::math::Vec2) {
        if (crossed.size() == kMaxTraversedCells) {
            throw std::invalid_argument("A grid traversal lists at most 65536 cells.");
        }

        crossed.emplace_back(cell, distance);
        return true;
    });
    // clang-format on

    lua_createtable(L, static_cast<int>(crossed.size()), 0);
    for (std::size_t index = 0; index < crossed.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua_pushinteger(L, crossed[index].first.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, crossed[index].first.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, crossed[index].second);
        lua_setfield(L, -2, "distance");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int GridBinding::line(lua_State* L) {
    std::vector<haylen::spatial2d::Cell> cells;
    haylen::spatial2d::Bresenham::line(readCell(L, 1), readCell(L, 3), cells);
    pushCells(L, cells);
    return 1;
}

int GridBinding::circle(lua_State* L) {
    std::vector<haylen::spatial2d::Cell> cells;
    haylen::spatial2d::Bresenham::circle(readCell(L, 1), lua::Stack::read<int>(L, 3), cells);
    pushCells(L, cells);
    return 1;
}

// Lists the cells visible from a cell with `fieldOfView(grid, x, y, radius)`, where solid cells block sight and cells outside the grid count as solid.
int GridBinding::fieldOfView(lua_State* L) {
    const haylen::spatial2d::CellGrid& grid = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    const haylen::spatial2d::Cell origin = readCell(L, 2);
    const auto radius = lua::Stack::read<int>(L, 4);

    // Cells on the axes and diagonals come twice, so a mark per grid cell within reach of the origin keeps each one once.
    const std::int64_t reach = std::max(radius, 0);
    const std::int64_t left = std::max<std::int64_t>(0, origin.x - reach);
    const std::int64_t top = std::max<std::int64_t>(0, origin.y - reach);
    const std::int64_t columns = std::max<std::int64_t>(0, std::min<std::int64_t>(grid.getWidth(), origin.x + reach + 1) - left);
    const std::int64_t rows = std::max<std::int64_t>(0, std::min<std::int64_t>(grid.getHeight(), origin.y + reach + 1) - top);
    std::vector<std::uint8_t> revealed(static_cast<std::size_t>(columns * rows), 0);
    std::vector<haylen::spatial2d::Cell> cells;
    // clang-format off
    haylen::spatial2d::FieldOfView::compute(origin, radius, [&grid](haylen::spatial2d::Cell cell) { return grid.isSolid(cell); }, [&](haylen::spatial2d::Cell cell) {
        if (!grid.contains(cell)) {
            return;
        }

        const auto mark = static_cast<std::size_t>((cell.y - top) * columns + (cell.x - left));
        if (revealed[mark] == 0) {
            revealed[mark] = 1;
            cells.push_back(cell);
        }
    });
    // clang-format on
    pushCells(L, cells);
    return 1;
}

// Returns the outline of the area visible from a point with `visibilityPolygon(origin, walls, bounds)`, as a list of `Vec2`.
int GridBinding::visibilityPolygon(lua_State* L) {
    haylen::spatial2d::VisibilityPolygon polygon;
    lua::Stack::push(L, polygon.compute(lua::Stack::read<haylen::math::Vec2>(L, 1), lua::Stack::read<std::vector<haylen::math::Segment>>(L, 2), lua::Stack::read<haylen::math::Rect>(L, 3)));
    return 1;
}

// Lists the cells connected to a cell that share its value with `floodFill(grid, x, y[, {diagonal = false, value = n}])`, and paints them with the value when one is given.
int GridBinding::floodFill(lua_State* L) {
    haylen::spatial2d::CellGrid& grid = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    const haylen::spatial2d::Cell start = readCell(L, 2);
    bool diagonal = false;
    std::optional<std::int32_t> paint;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kFillFields});
        lua::Table::readField(L, 4, "diagonal", diagonal);
        lua::Table::readField(L, 4, "value", paint);
    }

    std::vector<haylen::spatial2d::Cell> cells;
    haylen::spatial2d::FloodFill().fill(grid, start, diagonal, cells);

    if (paint) {
        for (const haylen::spatial2d::Cell cell : cells) {
            grid.set(cell, *paint);
        }
    }

    pushCells(L, cells);
    return 1;
}

// Labels the regions of a grid with `components(grid[, {diagonal = false, background = n}])` and returns a `CellGrid` of region numbers and the region count.
int GridBinding::components(lua_State* L) {
    const haylen::spatial2d::CellGrid& grid = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    haylen::spatial2d::ConnectedComponents::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kRegionFields});
        lua::Table::readField(L, 2, "diagonal", options.diagonal);
        lua::Table::readField(L, 2, "background", options.background);
    }

    haylen::spatial2d::CellGrid& labels = lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, grid.getWidth(), grid.getHeight(), 0);
    lua::Stack::push(L, haylen::spatial2d::ConnectedComponents::label(grid, options, labels));
    return 2;
}

void GridBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newCellGrid", &lua::Binding::native<&newCellGrid>}, {"newUnionFind", &lua::Binding::native<&newUnionFind>}, {"raycastGrid", &lua::Binding::native<&raycastGrid>}, {"traverseGrid", &lua::Binding::native<&traverseGrid>}, {"line", &lua::Binding::native<&line>}, {"circle", &lua::Binding::native<&circle>}, {"fieldOfView", &lua::Binding::native<&fieldOfView>}, {"visibilityPolygon", &lua::Binding::native<&visibilityPolygon>}, {"floodFill", &lua::Binding::native<&floodFill>}, {"components", &lua::Binding::native<&components>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void GridBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::spatial2d::CellGrid>(L).function("get", &lua::Binding::native<&gridGet>).function("set", &lua::Binding::native<&gridSet>).function("fill", &lua::Binding::native<&gridFill>).function("contains", &lua::Binding::native<&gridContains>).property("width", &gridWidth).property("height", &gridHeight).install();
    lua::ClassBuilder<haylen::spatial2d::UnionFind>(L).function("add", &lua::Binding::native<&unionAdd>).function("find", &lua::Binding::native<&unionFind>).function("unite", &lua::Binding::native<&unionUnite>).function("connected", &lua::Binding::native<&unionConnected>).function("setSize", &lua::Binding::native<&unionSetSize>).function("reset", &lua::Binding::native<&unionReset>).property("size", &unionSize).property("setCount", &unionSetCount).install();
}

} // namespace haylen::lua::spatial2d
