#include "2d/navigation/NavGridBinding.hpp"

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
#include "haylen/math/Ray2.hpp"
#include "math/RaycastBinding.hpp"

namespace haylen::lua {

template <> struct Type<lua::navigation2d::ScriptedGrid> {
    static constexpr const char* name = "haylen.NavGrid";
    using Storage = lua::navigation2d::ScriptedGrid;
};

template <> struct Type<haylen::navigation2d::DijkstraMap> {
    static constexpr const char* name = "haylen.DijkstraMap";
    using Storage = haylen::navigation2d::DijkstraMap;
};

template <> struct Type<haylen::navigation2d::FlowField> {
    static constexpr const char* name = "haylen.FlowField";
    using Storage = haylen::navigation2d::FlowField;
};

template <> struct Type<haylen::navigation2d::HierarchicalPathfinder> {
    static constexpr const char* name = "haylen.HierarchicalPathfinder";
    using Storage = haylen::navigation2d::HierarchicalPathfinder;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::navigation2d::Grid::Topology> {
    static std::optional<haylen::navigation2d::Grid::Topology> fromName(std::string_view name) {
        return haylen::navigation2d::Grid::topologyFromName(name);
    }

    static std::string_view name(haylen::navigation2d::Grid::Topology value) {
        return haylen::navigation2d::Grid::topologyName(value);
    }
};

template <> struct EnumNames<haylen::navigation2d::Grid::Heuristic> {
    static std::optional<haylen::navigation2d::Grid::Heuristic> fromName(std::string_view name) {
        return haylen::navigation2d::Grid::heuristicFromName(name);
    }

    static std::string_view name(haylen::navigation2d::Grid::Heuristic value) {
        return haylen::navigation2d::Grid::heuristicName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::navigation2d {

haylen::navigation2d::Grid& NavGridBinding::check(lua_State* L) {
    return lua::Userdata::check<ScriptedGrid>(L, 1).grid;
}

haylen::navigation2d::Grid::Cell NavGridBinding::readCell(lua_State* L, int first) {
    return {lua::Stack::read<int>(L, first), lua::Stack::read<int>(L, first + 1)};
}

haylen::navigation2d::GridSearch::Options NavGridBinding::readPathOptions(lua_State* L, int index) {
    haylen::navigation2d::GridSearch::Options options;
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

bool NavGridBinding::readDiagonal(lua_State* L, int index) {
    bool diagonal = true;
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TTABLE);
        lua::Table::checkFields(L, index, {kMapFields});
        lua::Table::readField(L, index, "diagonal", diagonal);
    }

    return diagonal;
}

std::vector<haylen::navigation2d::DijkstraMap::Source> NavGridBinding::readSources(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int list = lua_absindex(L, index);
    std::vector<haylen::navigation2d::DijkstraMap::Source> sources;
    const auto count = static_cast<lua_Integer>(luaL_len(L, list));
    for (lua_Integer element = 1; element <= count; ++element) {
        lua_rawgeti(L, list, element);
        haylen::navigation2d::DijkstraMap::Source source{.cell = lua::Stack::read<haylen::navigation2d::Grid::Cell>(L, -1)};
        if (lua_getfield(L, -1, "value") != LUA_TNIL) {
            source.value = lua::Stack::read<float>(L, -1);
        }

        lua_pop(L, 2);
        sources.push_back(source);
    }

    return sources;
}

void NavGridBinding::pushPath(lua_State* L, std::span<const haylen::navigation2d::Grid::Cell> path) {
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

template <typename T, typename... Args> T& NavGridBinding::pushOwned(lua_State* L, int gridIndex, Args&&... args) {
    const int grid = lua_absindex(L, gridIndex);
    T& object = lua::Userdata::emplace<T>(L, std::forward<Args>(args)...);
    lua_pushvalue(L, grid);
    lua_setiuservalue(L, -2, 1);
    return object;
}

haylen::navigation2d::Grid& NavGridBinding::ownerGrid(lua_State* L) {
    lua_getiuservalue(L, 1, 1);
    return lua::Userdata::check<ScriptedGrid>(L, -1).grid;
}

std::shared_ptr<const haylen::navigation2d::Grid> NavGridBinding::snapshotOf(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);

    if (!self.snapshot) {
        self.snapshot = std::make_shared<const haylen::navigation2d::Grid>(self.grid);
    }

    return self.snapshot;
}

// Every coroutine that awaits the promise receives its own copy of the value, which keeps the grid of the owner alive.
template <typename T> void NavGridBinding::settleOwned(const lua::Promise& promise, const std::shared_ptr<lua::Reference>& owner, haylen::core::JobSystem::Result<T> result) {
    // clang-format off
    promise.settle(std::move(result), [owner](lua_State* state, const T& value) {
        owner->push(state);
        pushOwned<T>(state, -1, value);
        lua_remove(state, -2);
    });
    // clang-format on
}

// Creates a grid with `newGrid(width, height[, {topology = 'square', staggerX = false, staggerEven = false}])`.
int NavGridBinding::newGrid(lua_State* L) {
    haylen::navigation2d::Grid::Layout layout;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kGridFields});
        lua::Table::readField(L, 3, "topology", layout.topology);
        lua::Table::readField(L, 3, "staggerX", layout.staggerX);
        lua::Table::readField(L, 3, "staggerEven", layout.staggerEven);
    }

    lua::Userdata::emplace<ScriptedGrid>(L, ScriptedGrid{.grid = haylen::navigation2d::Grid(lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), layout), .search = {}});
    return 1;
}

int NavGridBinding::getWidth(lua_State* L) {
    lua::Stack::push(L, check(L).getWidth());
    return 1;
}

int NavGridBinding::getHeight(lua_State* L) {
    lua::Stack::push(L, check(L).getHeight());
    return 1;
}

int NavGridBinding::getTopology(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().topology);
    return 1;
}

int NavGridBinding::isStaggerX(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().staggerX);
    return 1;
}

int NavGridBinding::isStaggerEven(lua_State* L) {
    lua::Stack::push(L, check(L).getLayout().staggerEven);
    return 1;
}

int NavGridBinding::hasUniformCost(lua_State* L) {
    lua::Stack::push(L, check(L).hasUniformCost());
    return 1;
}

int NavGridBinding::getExpandedCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedGrid>(L, 1).search.getExpandedCount());
    return 1;
}

int NavGridBinding::contains(lua_State* L) {
    lua::Stack::push(L, check(L).contains(readCell(L, 2)));
    return 1;
}

int NavGridBinding::setWalkable(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    self.grid.setWalkable(readCell(L, 2), lua::Stack::read<bool>(L, 4));
    self.snapshot.reset();
    return 0;
}

int NavGridBinding::isWalkable(lua_State* L) {
    lua::Stack::push(L, check(L).isWalkable(readCell(L, 2)));
    return 1;
}

int NavGridBinding::setCost(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    self.grid.setCost(readCell(L, 2), lua::Stack::read<float>(L, 4));
    self.snapshot.reset();
    return 0;
}

int NavGridBinding::getCost(lua_State* L) {
    lua::Stack::push(L, check(L).getCost(readCell(L, 2)));
    return 1;
}

// Estimates the walk between two cells with `estimate(fromX, fromY, toX, toY, heuristic)`.
int NavGridBinding::estimate(lua_State* L) {
    lua::Stack::push(L, check(L).estimate(readCell(L, 2), readCell(L, 4), lua::Stack::read<haylen::navigation2d::Grid::Heuristic>(L, 6)));
    return 1;
}

// Finds a path with `findPath(startX, startY, goalX, goalY[, options])` and returns its cells and cost, or `nil` when there is none.
int NavGridBinding::findPath(lua_State* L) {
    ScriptedGrid& self = lua::Userdata::check<ScriptedGrid>(L, 1);
    const std::span<const haylen::navigation2d::Grid::Cell> path = self.search.findPath(self.grid, readCell(L, 2), readCell(L, 4), readPathOptions(L, 6));
    pushPath(L, path);

    if (path.empty()) {
        return 1;
    }

    lua::Stack::push(L, self.search.getCost());
    return 2;
}

// Returns a promise for the path of `findPath`, searched on the snapshot of the grid in the background.
int NavGridBinding::findPathAsync(lua_State* L) {
    const haylen::navigation2d::Grid::Cell start = readCell(L, 2);
    const haylen::navigation2d::Grid::Cell goal = readCell(L, 4);
    const haylen::navigation2d::GridSearch::Options options = readPathOptions(L, 6);
    haylen::navigation2d::GridSearch::requireValid(check(L), options);
    const std::shared_ptr<const haylen::navigation2d::Grid> grid = snapshotOf(L);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);

    // clang-format off
    engine.getJobs().run([grid, start, goal, options] {
        haylen::navigation2d::GridSearch search;
        const std::span<const haylen::navigation2d::Grid::Cell> path = search.findPath(*grid, start, goal, options);
        return std::vector<haylen::navigation2d::Grid::Cell>(path.begin(), path.end());
    }, [promise](haylen::core::JobSystem::Result<std::vector<haylen::navigation2d::Grid::Cell>> result) {
        promise.settle(std::move(result), &pushPath);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int NavGridBinding::lineOfSight(lua_State* L) {
    lua::Stack::push(L, check(L).hasLineOfSight(readCell(L, 2), readCell(L, 4)));
    return 1;
}

int NavGridBinding::smoothPath(lua_State* L) {
    const haylen::navigation2d::Grid& grid = check(L);
    std::vector<haylen::navigation2d::Grid::Cell> path = lua::Stack::read<std::vector<haylen::navigation2d::Grid::Cell>>(L, 2);
    grid.smoothPath(path);
    lua::Stack::push(L, path);
    return 1;
}

// Casts a ray over the blocked cells of a square grid with `raycast(from, to, cellSize)`, where the cell size is a number or a `{width, height}` pair.
int NavGridBinding::raycast(lua_State* L) {
    const haylen::navigation2d::Grid& grid = check(L);
    if (grid.getLayout().topology != haylen::navigation2d::Grid::Topology::Square) {
        return luaL_error(L, "Grid ray casts need a square grid.");
    }

    const haylen::math::Ray2 ray = haylen::math::Ray2::between(lua::Stack::read<haylen::math::Vec2>(L, 2), lua::Stack::read<haylen::math::Vec2>(L, 3));
    const haylen::math::Vec2 cellSize = lua_type(L, 4) == LUA_TNUMBER ? haylen::math::Vec2{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 4)} : lua::Stack::read<haylen::math::Vec2>(L, 4);
    const std::optional<haylen::spatial2d::GridRay::Hit> hit = haylen::spatial2d::GridRay::cast(ray, cellSize, [&grid](haylen::spatial2d::Cell cell) { return !grid.isWalkable({cell.x, cell.y}); });
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

// Computes a Dijkstra map with `dijkstraMap(sources[, {diagonal = true}])`, where `sources` is a list of `{x, y, value}` cells.
int NavGridBinding::dijkstraMap(lua_State* L) {
    const haylen::navigation2d::Grid& grid = check(L);
    const std::vector<haylen::navigation2d::DijkstraMap::Source> sources = readSources(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    haylen::navigation2d::DijkstraMap& map = pushOwned<haylen::navigation2d::DijkstraMap>(L, 1);
    map.compute(grid, sources, diagonal);
    return 1;
}

// Returns a promise for the map of `dijkstraMap`, computed on the snapshot of the grid in the background.
int NavGridBinding::dijkstraMapAsync(lua_State* L) {
    std::vector<haylen::navigation2d::DijkstraMap::Source> sources = readSources(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    haylen::navigation2d::DijkstraMap::requireValid(sources);
    const std::shared_ptr<const haylen::navigation2d::Grid> grid = snapshotOf(L);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, sources = std::move(sources), diagonal] {
        haylen::navigation2d::DijkstraMap map;
        map.compute(*grid, sources, diagonal);
        return map;
    }, [promise, owner](haylen::core::JobSystem::Result<haylen::navigation2d::DijkstraMap> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Computes a flow field toward the nearest of the goals with `flowField(goals[, {diagonal = true}])`.
int NavGridBinding::flowField(lua_State* L) {
    const haylen::navigation2d::Grid& grid = check(L);
    const std::vector<haylen::navigation2d::Grid::Cell> goals = lua::Stack::read<std::vector<haylen::navigation2d::Grid::Cell>>(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    haylen::navigation2d::FlowField& field = pushOwned<haylen::navigation2d::FlowField>(L, 1);
    field.compute(grid, goals, diagonal);
    return 1;
}

// Returns a promise for the field of `flowField`, computed on the snapshot of the grid in the background.
int NavGridBinding::flowFieldAsync(lua_State* L) {
    std::vector<haylen::navigation2d::Grid::Cell> goals = lua::Stack::read<std::vector<haylen::navigation2d::Grid::Cell>>(L, 2);
    const bool diagonal = readDiagonal(L, 3);
    const std::shared_ptr<const haylen::navigation2d::Grid> grid = snapshotOf(L);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, goals = std::move(goals), diagonal] {
        haylen::navigation2d::FlowField field;
        field.compute(*grid, goals, diagonal);
        return field;
    }, [promise, owner](haylen::core::JobSystem::Result<haylen::navigation2d::FlowField> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Reads `{clusterSize = 16, diagonal = true}`, where `nil` gives the defaults.
haylen::navigation2d::HierarchicalPathfinder::Options NavGridBinding::readHierarchyOptions(lua_State* L, int index) {
    haylen::navigation2d::HierarchicalPathfinder::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kHierarchyFields});
    lua::Table::readField(L, index, "clusterSize", options.clusterSize);
    lua::Table::readField(L, index, "diagonal", options.diagonal);
    return options;
}

// Builds a hierarchical path finder over the grid with `hierarchicalPathfinder([options])`.
int NavGridBinding::hierarchicalPathfinder(lua_State* L) {
    const haylen::navigation2d::Grid& grid = check(L);
    pushOwned<haylen::navigation2d::HierarchicalPathfinder>(L, 1, grid, readHierarchyOptions(L, 2));
    return 1;
}

// Returns a promise for the path finder of `hierarchicalPathfinder`, built from the snapshot of the grid in the background.
int NavGridBinding::hierarchicalPathfinderAsync(lua_State* L) {
    const haylen::navigation2d::HierarchicalPathfinder::Options options = readHierarchyOptions(L, 2);
    haylen::navigation2d::HierarchicalPathfinder::requireValid(check(L), options);
    const std::shared_ptr<const haylen::navigation2d::Grid> grid = snapshotOf(L);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    const auto owner = std::make_shared<lua::Reference>(L, 1);

    // clang-format off
    engine.getJobs().run([grid, options] {
        return haylen::navigation2d::HierarchicalPathfinder(*grid, options);
    }, [promise, owner](haylen::core::JobSystem::Result<haylen::navigation2d::HierarchicalPathfinder> result) {
        settleOwned(promise, owner, std::move(result));
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int NavGridBinding::mapValue(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1).getValue(readCell(L, 2)));
    return 1;
}

int NavGridBinding::mapNext(lua_State* L) {
    const haylen::navigation2d::DijkstraMap& map = lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1);
    const haylen::navigation2d::Grid::Cell cell = readCell(L, 2);
    const std::optional<haylen::navigation2d::Grid::Cell> next = map.getNext(ownerGrid(L), cell);
    if (!next) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, next->x);
    lua::Stack::push(L, next->y);
    return 2;
}

int NavGridBinding::mapFlee(lua_State* L) {
    haylen::navigation2d::DijkstraMap& map = lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1);
    const float coefficient = lua_isnoneornil(L, 2) ? -1.2F : lua::Stack::read<float>(L, 2);
    map.flee(ownerGrid(L), coefficient);
    return 0;
}

int NavGridBinding::mapWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1).getWidth());
    return 1;
}

int NavGridBinding::mapHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1).getHeight());
    return 1;
}

// Returns every value row by row, which suits drawing the map.
int NavGridBinding::mapValues(lua_State* L) {
    const std::span<const float> values = lua::Userdata::check<haylen::navigation2d::DijkstraMap>(L, 1).getValues();
    lua_createtable(L, static_cast<int>(values.size()), 0);
    for (std::size_t index = 0; index < values.size(); ++index) {
        lua_pushnumber(L, values[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int NavGridBinding::fieldNext(lua_State* L) {
    const std::optional<haylen::navigation2d::Grid::Cell> next = lua::Userdata::check<haylen::navigation2d::FlowField>(L, 1).getNext(readCell(L, 2));
    if (!next) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, next->x);
    lua::Stack::push(L, next->y);
    return 2;
}

int NavGridBinding::fieldDirection(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::FlowField>(L, 1).getDirection(readCell(L, 2)));
    return 1;
}

int NavGridBinding::fieldDistance(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::FlowField>(L, 1).getDistance(readCell(L, 2)));
    return 1;
}

int NavGridBinding::fieldWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::FlowField>(L, 1).getWidth());
    return 1;
}

int NavGridBinding::fieldHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::FlowField>(L, 1).getHeight());
    return 1;
}

int NavGridBinding::hierarchyFindPath(lua_State* L) {
    haylen::navigation2d::HierarchicalPathfinder& hierarchy = lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1);
    const haylen::navigation2d::Grid::Cell start = readCell(L, 2);
    const haylen::navigation2d::Grid::Cell goal = readCell(L, 4);
    const std::span<const haylen::navigation2d::Grid::Cell> path = hierarchy.findPath(ownerGrid(L), start, goal);
    pushPath(L, path);

    if (path.empty()) {
        return 1;
    }

    lua::Stack::push(L, hierarchy.getCost());
    return 2;
}

// Rebuilds the clusters of the cells from `x1`, `y1` to `x2`, `y2` with `update(x1, y1, x2, y2)`, or of one cell with `update(x, y)`.
int NavGridBinding::hierarchyUpdate(lua_State* L) {
    const haylen::navigation2d::Grid::Cell first = readCell(L, 2);
    const haylen::navigation2d::Grid::Cell last = lua_isnoneornil(L, 4) ? first : readCell(L, 4);
    lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1).update(ownerGrid(L), first, last);
    return 0;
}

int NavGridBinding::hierarchyRebuild(lua_State* L) {
    lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1).rebuild(ownerGrid(L));
    return 0;
}

int NavGridBinding::hierarchyNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1).getNodeCount());
    return 1;
}

int NavGridBinding::hierarchyClusterSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1).getOptions().clusterSize);
    return 1;
}

int NavGridBinding::hierarchyDiagonal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::navigation2d::HierarchicalPathfinder>(L, 1).getOptions().diagonal);
    return 1;
}

void NavGridBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newGrid", &lua::Binding::native<&newGrid>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

void NavGridBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedGrid>(L).property("width", &getWidth).property("height", &getHeight).property("topology", &getTopology).property("staggerX", &isStaggerX).property("staggerEven", &isStaggerEven).property("uniformCost", &hasUniformCost).property("expandedCount", &getExpandedCount).function("contains", &lua::Binding::native<&contains>).function("setWalkable", &lua::Binding::native<&setWalkable>).function("walkable", &lua::Binding::native<&isWalkable>).function("setCost", &lua::Binding::native<&setCost>).function("cost", &lua::Binding::native<&getCost>).function("estimate", &lua::Binding::native<&estimate>).function("findPath", &lua::Binding::native<&findPath>).function("findPathAsync", &lua::Binding::native<&findPathAsync>).function("lineOfSight", &lua::Binding::native<&lineOfSight>).function("smoothPath", &lua::Binding::native<&smoothPath>).function("raycast", &lua::Binding::native<&raycast>).function("dijkstraMap", &lua::Binding::native<&dijkstraMap>).function("dijkstraMapAsync", &lua::Binding::native<&dijkstraMapAsync>).function("flowField", &lua::Binding::native<&flowField>).function("flowFieldAsync", &lua::Binding::native<&flowFieldAsync>).function("hierarchicalPathfinder", &lua::Binding::native<&hierarchicalPathfinder>).function("hierarchicalPathfinderAsync", &lua::Binding::native<&hierarchicalPathfinderAsync>).install();
    lua::ClassBuilder<haylen::navigation2d::DijkstraMap>(L).function("value", &lua::Binding::native<&mapValue>).function("next", &lua::Binding::native<&mapNext>).function("flee", &lua::Binding::native<&mapFlee>).function("values", &lua::Binding::native<&mapValues>).property("width", &mapWidth).property("height", &mapHeight).install();
    lua::ClassBuilder<haylen::navigation2d::FlowField>(L).function("next", &lua::Binding::native<&fieldNext>).function("direction", &lua::Binding::native<&fieldDirection>).function("distance", &lua::Binding::native<&fieldDistance>).property("width", &fieldWidth).property("height", &fieldHeight).install();
    lua::ClassBuilder<haylen::navigation2d::HierarchicalPathfinder>(L).function("findPath", &lua::Binding::native<&hierarchyFindPath>).function("update", &lua::Binding::native<&hierarchyUpdate>).function("rebuild", &lua::Binding::native<&hierarchyRebuild>).property("nodeCount", &hierarchyNodeCount).property("clusterSize", &hierarchyClusterSize).property("diagonal", &hierarchyDiagonal).install();
}

} // namespace haylen::lua::navigation2d
