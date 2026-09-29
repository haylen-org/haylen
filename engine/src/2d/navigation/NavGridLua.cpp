#include "2d/navigation/NavGridLua.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "2d/navigation/GridCellConverter.hpp"
#include "2d/navigation/ScriptedGrid.hpp"
#include "haylen/2d/navigation/FlowField.hpp"
#include "haylen/2d/navigation/HierarchicalPathfinder.hpp"
#include "haylen/2d/spatial/GridRay.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "math/RaycastLua.hpp"

namespace haylen::lua {

template <> struct Type<navigation2d::ScriptedGrid> {
    static constexpr const char* name = "haylen.NavGrid";
    using Storage = navigation2d::ScriptedGrid;
};

template <> struct Type<navigation2d::DijkstraMap> {
    static constexpr const char* name = "haylen.DijkstraMap";
    using Storage = navigation2d::DijkstraMap;
};

template <> struct Type<navigation2d::FlowField> {
    static constexpr const char* name = "haylen.FlowField";
    using Storage = navigation2d::FlowField;
};

template <> struct Type<navigation2d::HierarchicalPathfinder> {
    static constexpr const char* name = "haylen.HierarchicalPathfinder";
    using Storage = navigation2d::HierarchicalPathfinder;
};

template <> struct EnumNames<navigation2d::Grid::Topology> {
    static std::optional<navigation2d::Grid::Topology> fromName(std::string_view name) {
        if (name == "square") {
            return navigation2d::Grid::Topology::Square;
        }
        if (name == "hexagonal") {
            return navigation2d::Grid::Topology::Hexagonal;
        }
        if (name == "staggered") {
            return navigation2d::Grid::Topology::Staggered;
        }
        return std::nullopt;
    }
    static std::string_view name(navigation2d::Grid::Topology value) {
        return value == navigation2d::Grid::Topology::Hexagonal ? "hexagonal" : (value == navigation2d::Grid::Topology::Staggered ? "staggered" : "square");
    }
};

template <> struct EnumNames<navigation2d::Grid::Heuristic> {
    static std::optional<navigation2d::Grid::Heuristic> fromName(std::string_view name) {
        if (name == "manhattan") {
            return navigation2d::Grid::Heuristic::Manhattan;
        }
        if (name == "octile") {
            return navigation2d::Grid::Heuristic::Octile;
        }
        if (name == "euclidean") {
            return navigation2d::Grid::Heuristic::Euclidean;
        }
        if (name == "chebyshev") {
            return navigation2d::Grid::Heuristic::Chebyshev;
        }
        return std::nullopt;
    }
    static std::string_view name(navigation2d::Grid::Heuristic value) {
        switch (value) {
        case navigation2d::Grid::Heuristic::Manhattan:
            return "manhattan";
        case navigation2d::Grid::Heuristic::Octile:
            return "octile";
        case navigation2d::Grid::Heuristic::Euclidean:
            return "euclidean";
        case navigation2d::Grid::Heuristic::Chebyshev:
            break;
        }
        return "chebyshev";
    }
};

} // namespace haylen::lua

namespace haylen::navigation2d {

Grid& NavGridLua::check(lua_State* L) {
    return lua::Userdata::check<ScriptedGrid>(L, 1).grid;
}

Grid::Cell NavGridLua::readCell(lua_State* L, int first) {
    return {lua::Stack::read<int>(L, first), lua::Stack::read<int>(L, first + 1)};
}

GridSearch::Options NavGridLua::readPathOptions(lua_State* L, int index) {
    GridSearch::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kPathFields});
    lua::Table::readField(L, index, "diagonal", options.diagonal);
    lua::Table::readField(L, index, "smooth", options.smooth);
    lua::Table::readField(L, index, "heuristic", options.heuristic);
    lua::Table::readField(L, index, "weight", options.weight);
    lua::Table::readField(L, index, "jumpPoint", options.jumpPoint);
    return options;
}

bool NavGridLua::readDiagonal(lua_State* L, int index) {
    bool diagonal = true;
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kMapFields});
        lua::Table::readField(L, index, "diagonal", diagonal);
    }
    return diagonal;
}

std::vector<DijkstraMap::Source> NavGridLua::readSources(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int list = lua_absindex(L, index);
    std::vector<DijkstraMap::Source> sources;
    const auto count = static_cast<lua_Integer>(luaL_len(L, list));
    for (lua_Integer element = 1; element <= count; ++element) {
        lua_rawgeti(L, list, element);
        DijkstraMap::Source source{.cell = lua::Stack::read<Grid::Cell>(L, -1)};
        if (lua_getfield(L, -1, "value") != LUA_TNIL) {
            source.value = lua::Stack::read<float>(L, -1);
        }
        lua_pop(L, 2);
        sources.push_back(source);
    }
    return sources;
}

void NavGridLua::pushPath(lua_State* L, std::span<const Grid::Cell> path) {
    if (path.empty()) {
        lua_pushnil(L);
        return;
    }
    lua_createtable(L, static_cast<int>(path.size()), 0);
    for (std::size_t index = 0; index < path.size(); ++index) {
        lua::Stack::push(L, path[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

template <typename T, typename... Args> T& NavGridLua::pushOwned(lua_State* L, int gridIndex, Args&&... args) {
    const int grid = lua_absindex(L, gridIndex);
    T& object = lua::Userdata::emplace<T>(L, std::forward<Args>(args)...);
    lua_pushvalue(L, grid);
    lua_setiuservalue(L, -2, 1);
    return object;
}

Grid& NavGridLua::ownerGrid(lua_State* L) {
    lua_getiuservalue(L, 1, 1);
    return lua::Userdata::check<ScriptedGrid>(L, -1).grid;
}

std::shared_ptr<const Grid> NavGridLua::snapshotOf(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    if (!self.snapshot) {
        self.snapshot = std::make_shared<const Grid>(self.grid);
    }
    return self.snapshot;
}

template <typename T> void NavGridLua::settleOwned(const lua::Promise& promise, const std::shared_ptr<lua::Reference>& owner, core::JobSystem::Result<T> result) {
    if (!result.isOk()) {
        promise.reject(result.error);
        return;
    }
    // clang-format off
    promise.resolveWith([owner, value = std::make_shared<const T>(std::move(*result.value))](lua_State* state) {
        owner->push(state);
        pushOwned<T>(state, -1, *value);
        lua_remove(state, -2);
    });
    // clang-format on
}

// Creates a grid with newGrid(width, height[, {topology = 'square', staggerX = false, staggerEven = false}]).
int NavGridLua::newGrid(lua_State* L) {
    Grid::Layout layout;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kGridFields});
        lua::Table::readField(L, 3, "topology", layout.topology);
        lua::Table::readField(L, 3, "staggerX", layout.staggerX);
        lua::Table::readField(L, 3, "staggerEven", layout.staggerEven);
    }
    lua::Userdata::emplace<ScriptedGrid>(L, ScriptedGrid{.grid = Grid(lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), layout), .search = {}});
    return 1;
}

int NavGridLua::getWidth(lua_State* L) {
    lua::Stack::push(L, check(L).getWidth());
    return 1;
}

int NavGridLua::getHeight(lua_State* L) {
    lua::Stack::push(L, check(L).getHeight());
    return 1;
}

int NavGridLua::getTopology(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().topology);
    return 1;
}

int NavGridLua::isStaggerX(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().staggerX);
    return 1;
}

int NavGridLua::isStaggerEven(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().staggerEven);
    return 1;
}

int NavGridLua::hasUniformCost(lua_State* L) {
    lua::Stack::push(L, check(L).hasUniformCost());
    return 1;
}

int NavGridLua::getExpandedCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedGrid>(L, 1).search.getExpandedCount());
    return 1;
}

int NavGridLua::contains(lua_State* L) {
    lua::Stack::push(L, check(L).contains(readCell(L, 2)));
    return 1;
}

int NavGridLua::setWalkable(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    self.grid.setWalkable(readCell(L, 2), lua::Stack::read<bool>(L, 4));
    self.snapshot.reset();
    return 0;
}

int NavGridLua::isWalkable(lua_State* L) {
    lua::Stack::push(L, check(L).isWalkable(readCell(L, 2)));
    return 1;
}

int NavGridLua::setCost(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    self.grid.setCost(readCell(L, 2), lua::Stack::read<float>(L, 4));
    self.snapshot.reset();
    return 0;
}

int NavGridLua::getCost(lua_State* L) {
    lua::Stack::push(L, check(L).getCost(readCell(L, 2)));
    return 1;
}

// Estimates the walk between two cells with estimate(fromX, fromY, toX, toY, heuristic).
int NavGridLua::estimate(lua_State* L) {
    lua::Stack::push(L, check(L).estimate(readCell(L, 2), readCell(L, 4), lua::Stack::read<Grid::Heuristic>(L, 6)));
    return 1;
}

// Finds a path with findPath(startX, startY, goalX, goalY[, options]) and returns its cells and cost, or nil when there is none.
int NavGridLua::findPath(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    const std::span<const Grid::Cell> path = self.search.findPath(self.grid, readCell(L, 2), readCell(L, 4), readPathOptions(L, 6));
    pushPath(L, path);
    if (path.empty()) {
        return 1;
    }
    lua::Stack::push(L, self.search.getCost());
    return 2;
}

// Returns a promise for the path of findPath, searched on the snapshot of the grid in the background.
int NavGridLua::findPathAsync(lua_State* L) {
    const Grid::Cell start = readCell(L, 2);
    const Grid::Cell goal = readCell(L, 4);
    const GridSearch::Options options = readPathOptions(L, 6);
    GridSearch::requireValid(check(L), options);
    const std::shared_ptr<const Grid> grid = snapshotOf(L);
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);

    // clang-format off
    engine.getJobs().run([grid, start, goal, options] {
        GridSearch search;
        const std::span<const Grid::Cell> path = search.findPath(*grid, start, goal, options);
        return std::vector<Grid::Cell>(path.begin(), path.end());
    }, [promise](core::JobSystem::Result<std::vector<Grid::Cell>> result) {
        if (!result.isOk()) {
            promise.reject(result.error);
            return;
        }
        promise.resolveWith([path = std::move(*result.value)](lua_State* state) { pushPath(state, path); });
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int NavGridLua::lineOfSight(lua_State* L) {
    lua::Stack::push(L, check(L).hasLineOfSight(readCell(L, 2), readCell(L, 4)));
    return 1;
}

int NavGridLua::smoothPath(lua_State* L) {
    const Grid& grid = check(L);
    std::vector<Grid::Cell> path = lua::Stack::read<std::vector<Grid::Cell>>(L, 2);
    grid.smoothPath(path);
    lua::Stack::push(L, path);
    return 1;
}

// Casts a ray over the blocked cells of a square grid with raycast(from, to, cellSize), where the cell size is a number or a {width, height} pair.
int NavGridLua::raycast(lua_State* L) {
    const Grid& grid = check(L);
    if (grid.getLayout().topology != Grid::Topology::Square) {
        return luaL_error(L, "Grid ray casts need a square grid.");
    }
    const math::Ray ray = math::Ray::between(lua::Stack::read<math::Vec2>(L, 2), lua::Stack::read<math::Vec2>(L, 3));
    const math::Vec2 cellSize = lua_type(L, 4) == LUA_TNUMBER ? math::Vec2{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 4)} : lua::Stack::read<math::Vec2>(L, 4);
    const std::optional<spatial2d::GridRay::Hit> hit = spatial2d::GridRay::cast(ray, cellSize, [&grid](spatial2d::Cell cell) { return !grid.isWalkable({cell.x, cell.y}); });
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }
    math::RaycastLua::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, hit->cell.x);
    lua_setfield(L, -2, "column");
    lua_pushinteger(L, hit->cell.y);
    lua_setfield(L, -2, "row");
    return 1;
}

// Computes a Dijkstra map with dijkstraMap(sources[, {diagonal = true}]), where sources is a list of {x, y, value} cells.
int NavGridLua::dijkstraMap(lua_State* L) {
    const Grid& grid = check(L);
    const std::vector<DijkstraMap::Source> sources = readSources(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    DijkstraMap& map = pushOwned<DijkstraMap>(L, 1);
    map.compute(grid, sources, diagonal);
    return 1;
}

// Returns a promise for the map of dijkstraMap, computed on the snapshot of the grid in the background.
int NavGridLua::dijkstraMapAsync(lua_State* L) {
    std::vector<DijkstraMap::Source> sources = readSources(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    DijkstraMap::requireValid(sources);
    const std::shared_ptr<const Grid> grid = snapshotOf(L);
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, sources = std::move(sources), diagonal] {
        DijkstraMap map;
        map.compute(*grid, sources, diagonal);
        return map;
    }, [promise, owner](core::JobSystem::Result<DijkstraMap> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Computes a flow field toward the nearest of the goals with flowField(goals[, {diagonal = true}]).
int NavGridLua::flowField(lua_State* L) {
    const Grid& grid = check(L);
    const std::vector<Grid::Cell> goals = lua::Stack::read<std::vector<Grid::Cell>>(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    FlowField& field = pushOwned<FlowField>(L, 1);
    field.compute(grid, goals, diagonal);
    return 1;
}

// Returns a promise for the field of flowField, computed on the snapshot of the grid in the background.
int NavGridLua::flowFieldAsync(lua_State* L) {
    std::vector<Grid::Cell> goals = lua::Stack::read<std::vector<Grid::Cell>>(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    const std::shared_ptr<const Grid> grid = snapshotOf(L);
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, goals = std::move(goals), diagonal] {
        FlowField field;
        field.compute(*grid, goals, diagonal);
        return field;
    }, [promise, owner](core::JobSystem::Result<FlowField> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Reads {clusterSize = 16, diagonal = true}, where nil gives the defaults.
HierarchicalPathfinder::Options NavGridLua::readHierarchyOptions(lua_State* L, int index) {
    HierarchicalPathfinder::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kHierarchyFields});
    lua::Table::readField(L, index, "clusterSize", options.clusterSize);
    lua::Table::readField(L, index, "diagonal", options.diagonal);
    return options;
}

// Builds a hierarchical path finder over the grid with hierarchicalPathfinder([options]).
int NavGridLua::hierarchicalPathfinder(lua_State* L) {
    const Grid& grid = check(L);
    pushOwned<HierarchicalPathfinder>(L, 1, grid, readHierarchyOptions(L, 2));
    return 1;
}

// Returns a promise for the path finder of hierarchicalPathfinder, built from the snapshot of the grid in the background.
int NavGridLua::hierarchicalPathfinderAsync(lua_State* L) {
    const HierarchicalPathfinder::Options options = readHierarchyOptions(L, 2);
    HierarchicalPathfinder::requireValid(check(L), options);
    const std::shared_ptr<const Grid> grid = snapshotOf(L);
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, options] {
        return HierarchicalPathfinder(*grid, options);
    }, [promise, owner](core::JobSystem::Result<HierarchicalPathfinder> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int NavGridLua::mapValue(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<DijkstraMap>(L, 1).getValue(readCell(L, 2)));
    return 1;
}

int NavGridLua::mapNext(lua_State* L) {
    const DijkstraMap& map = lua::Userdata::check<DijkstraMap>(L, 1);
    const Grid::Cell cell = readCell(L, 2);
    const std::optional<Grid::Cell> next = map.getNext(ownerGrid(L), cell);
    if (!next) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, next->x);
    lua::Stack::push(L, next->y);
    return 2;
}

int NavGridLua::mapFlee(lua_State* L) {
    DijkstraMap& map = lua::Userdata::check<DijkstraMap>(L, 1);
    const float coefficient = lua_isnoneornil(L, 2) ? -1.2F : lua::Stack::read<float>(L, 2);
    map.flee(ownerGrid(L), coefficient);
    return 0;
}

int NavGridLua::mapWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<DijkstraMap>(L, 1).getWidth());
    return 1;
}

int NavGridLua::mapHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<DijkstraMap>(L, 1).getHeight());
    return 1;
}

// Returns every value row by row, which suits drawing the map.
int NavGridLua::mapValues(lua_State* L) {
    const std::span<const float> values = lua::Userdata::check<DijkstraMap>(L, 1).getValues();
    lua_createtable(L, static_cast<int>(values.size()), 0);
    for (std::size_t index = 0; index < values.size(); ++index) {
        lua_pushnumber(L, values[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int NavGridLua::fieldNext(lua_State* L) {
    const std::optional<Grid::Cell> next = lua::Userdata::check<FlowField>(L, 1).getNext(readCell(L, 2));
    if (!next) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, next->x);
    lua::Stack::push(L, next->y);
    return 2;
}

int NavGridLua::fieldDirection(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<FlowField>(L, 1).getDirection(readCell(L, 2)));
    return 1;
}

int NavGridLua::fieldDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<FlowField>(L, 1).getDistance(readCell(L, 2)));
    return 1;
}

int NavGridLua::fieldWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<FlowField>(L, 1).getWidth());
    return 1;
}

int NavGridLua::fieldHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<FlowField>(L, 1).getHeight());
    return 1;
}

int NavGridLua::hierarchyFindPath(lua_State* L) {
    HierarchicalPathfinder& hierarchy = lua::Userdata::check<HierarchicalPathfinder>(L, 1);
    const Grid::Cell start = readCell(L, 2);
    const Grid::Cell goal = readCell(L, 4);
    const std::span<const Grid::Cell> path = hierarchy.findPath(ownerGrid(L), start, goal);
    pushPath(L, path);
    if (path.empty()) {
        return 1;
    }
    lua::Stack::push(L, hierarchy.getCost());
    return 2;
}

// Rebuilds the clusters of the cells from x1, y1 to x2, y2 with update(x1, y1, x2, y2), or of one cell with update(x, y).
int NavGridLua::hierarchyUpdate(lua_State* L) {
    const Grid::Cell first = readCell(L, 2);
    const Grid::Cell last = lua_isnoneornil(L, 4) ? first : readCell(L, 4);
    lua::Userdata::check<HierarchicalPathfinder>(L, 1).update(ownerGrid(L), first, last);
    return 0;
}

int NavGridLua::hierarchyRebuild(lua_State* L) {
    lua::Userdata::check<HierarchicalPathfinder>(L, 1).rebuild(ownerGrid(L));
    return 0;
}

int NavGridLua::hierarchyNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<HierarchicalPathfinder>(L, 1).getNodeCount());
    return 1;
}

int NavGridLua::hierarchyClusterSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<HierarchicalPathfinder>(L, 1).getOptions().clusterSize);
    return 1;
}

int NavGridLua::hierarchyDiagonal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<HierarchicalPathfinder>(L, 1).getOptions().diagonal);
    return 1;
}

void NavGridLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newGrid", &lua::Binding::native<&newGrid>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void NavGridLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedGrid>(L).property("width", &getWidth).property("height", &getHeight).property("topology", &getTopology).property("staggerX", &isStaggerX).property("staggerEven", &isStaggerEven).property("uniformCost", &hasUniformCost).property("expandedCount", &getExpandedCount).function("contains", &lua::Binding::native<&contains>).function("setWalkable", &lua::Binding::native<&setWalkable>).function("walkable", &lua::Binding::native<&isWalkable>).function("setCost", &lua::Binding::native<&setCost>).function("cost", &lua::Binding::native<&getCost>).function("estimate", &lua::Binding::native<&estimate>).function("findPath", &lua::Binding::native<&findPath>).function("findPathAsync", &lua::Binding::native<&findPathAsync>).function("lineOfSight", &lua::Binding::native<&lineOfSight>).function("smoothPath", &lua::Binding::native<&smoothPath>).function("raycast", &lua::Binding::native<&raycast>).function("dijkstraMap", &lua::Binding::native<&dijkstraMap>).function("dijkstraMapAsync", &lua::Binding::native<&dijkstraMapAsync>).function("flowField", &lua::Binding::native<&flowField>).function("flowFieldAsync", &lua::Binding::native<&flowFieldAsync>).function("hierarchicalPathfinder", &lua::Binding::native<&hierarchicalPathfinder>).function("hierarchicalPathfinderAsync", &lua::Binding::native<&hierarchicalPathfinderAsync>).install();
    lua::ClassBuilder<DijkstraMap>(L).function("value", &lua::Binding::native<&mapValue>).function("next", &lua::Binding::native<&mapNext>).function("flee", &lua::Binding::native<&mapFlee>).function("values", &lua::Binding::native<&mapValues>).property("width", &mapWidth).property("height", &mapHeight).install();
    lua::ClassBuilder<FlowField>(L).function("next", &lua::Binding::native<&fieldNext>).function("direction", &lua::Binding::native<&fieldDirection>).function("distance", &lua::Binding::native<&fieldDistance>).property("width", &fieldWidth).property("height", &fieldHeight).install();
    lua::ClassBuilder<HierarchicalPathfinder>(L).function("findPath", &lua::Binding::native<&hierarchyFindPath>).function("update", &lua::Binding::native<&hierarchyUpdate>).function("rebuild", &lua::Binding::native<&hierarchyRebuild>).property("nodeCount", &hierarchyNodeCount).property("clusterSize", &hierarchyClusterSize).property("diagonal", &hierarchyDiagonal).install();
}

} // namespace haylen::navigation2d
