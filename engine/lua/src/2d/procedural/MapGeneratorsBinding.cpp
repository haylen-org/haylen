#include "2d/procedural/MapGeneratorsBinding.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "2d/procedural/Procedural2DBinding.hpp"
#include "2d/spatial/GridBinding.hpp"
#include "haylen/2d/procedural/Autotile.hpp"
#include "haylen/2d/tiled/WangSet.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<haylen::procedural2d::Maze> {
    static constexpr const char* name = "haylen.Maze";
    using Storage = haylen::procedural2d::Maze;
};

template <> struct Type<haylen::procedural2d::WaveFunctionCollapse::Rules> {
    static constexpr const char* name = "haylen.WaveFunctionCollapseRules";
    using Storage = haylen::procedural2d::WaveFunctionCollapse::Rules;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::procedural2d::Dungeon::Method> {
    static std::optional<haylen::procedural2d::Dungeon::Method> fromName(std::string_view name) {
        return haylen::procedural2d::Dungeon::methodFromName(name);
    }

    static std::string_view name(haylen::procedural2d::Dungeon::Method value) {
        return haylen::procedural2d::Dungeon::methodName(value);
    }
};

template <> struct EnumNames<haylen::procedural2d::Maze::Algorithm> {
    static std::optional<haylen::procedural2d::Maze::Algorithm> fromName(std::string_view name) {
        return haylen::procedural2d::Maze::algorithmFromName(name);
    }

    static std::string_view name(haylen::procedural2d::Maze::Algorithm value) {
        return haylen::procedural2d::Maze::algorithmName(value);
    }
};

template <> struct EnumNames<haylen::procedural2d::WaveFunctionCollapse::Direction> {
    static std::optional<haylen::procedural2d::WaveFunctionCollapse::Direction> fromName(std::string_view name) {
        return haylen::procedural2d::WaveFunctionCollapse::directionFromName(name);
    }

    static std::string_view name(haylen::procedural2d::WaveFunctionCollapse::Direction value) {
        return haylen::procedural2d::WaveFunctionCollapse::directionName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::procedural2d {

haylen::procedural2d::CellularAutomaton::Options MapGeneratorsBinding::readCaves(lua_State* L, int index) {
    haylen::procedural2d::CellularAutomaton::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kCaveFields});
    lua::Table::readField(L, index, "width", options.width);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "fillChance", options.fillChance);
    lua::Table::readField(L, index, "steps", options.steps);
    lua::Table::readField(L, index, "birthLimit", options.birthLimit);
    lua::Table::readField(L, index, "survivalLimit", options.survivalLimit);
    lua::Table::readField(L, index, "solidBorder", options.solidBorder);
    return options;
}

haylen::procedural2d::DrunkardWalk::Options MapGeneratorsBinding::readWalk(lua_State* L, int index) {
    haylen::procedural2d::DrunkardWalk::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kWalkFields});
    lua::Table::readField(L, index, "width", options.width);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "coverage", options.coverage);
    lua::Table::readField(L, index, "walkers", options.walkers);
    lua::Table::readField(L, index, "maxSteps", options.maxSteps);
    return options;
}

haylen::procedural2d::Dungeon::Options MapGeneratorsBinding::readDungeon(lua_State* L, int index) {
    haylen::procedural2d::Dungeon::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kDungeonFields});
    lua::Table::readField(L, index, "method", options.method);
    lua::Table::readField(L, index, "width", options.width);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "minimumRoomSize", options.minimumRoomSize);
    lua::Table::readField(L, index, "maximumRoomSize", options.maximumRoomSize);
    lua::Table::readField(L, index, "minimumLeafSize", options.minimumLeafSize);
    lua::Table::readField(L, index, "maximumRooms", options.maximumRooms);
    lua::Table::readField(L, index, "roomAttempts", options.roomAttempts);
    lua::Table::readField(L, index, "padding", options.padding);
    return options;
}

// Rules come from a sample grid, or from a tile count with `allow = {{first, second, 'right'}, ...}`. Weights list the tiles from tile 0 on.
haylen::procedural2d::WaveFunctionCollapse::Rules MapGeneratorsBinding::readRules(lua_State* L, int index) {
    std::optional<haylen::procedural2d::WaveFunctionCollapse::Rules> rules;
    if (lua_getfield(L, index, "sample") != LUA_TNIL) {
        bool periodic = false;
        lua::Table::readField(L, index, "periodicSample", periodic);
        rules.emplace(haylen::procedural2d::WaveFunctionCollapse::Rules::fromSample(lua::Userdata::check<haylen::spatial2d::CellGrid>(L, -1), periodic));
    } else {
        std::size_t tiles = 0;
        lua::Table::readField(L, index, "tiles", tiles);
        rules.emplace(tiles);
        if (lua_getfield(L, index, "allow") != LUA_TNIL) {
            lua::Table::requireType(L, "allow", LUA_TTABLE);
            const lua_Integer count = luaL_len(L, -1);
            for (lua_Integer pair = 1; pair <= count; ++pair) {
                lua_rawgeti(L, -1, pair);
                lua::Table::requireItemType(L, "allow", pair, LUA_TTABLE);
                lua_rawgeti(L, -1, 1);
                lua_rawgeti(L, -2, 2);
                lua_rawgeti(L, -3, 3);
                rules->allow(lua::Stack::read<std::size_t>(L, -3), lua::Stack::read<std::size_t>(L, -2), lua::Stack::read<haylen::procedural2d::WaveFunctionCollapse::Direction>(L, -1));
                lua_pop(L, 4);
            }
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    std::vector<float> weights;
    lua::Table::readField(L, index, "weights", weights);

    for (std::size_t tile = 0; tile < weights.size(); ++tile) {
        rules->setWeight(tile, weights[tile]);
    }

    return std::move(*rules);
}

haylen::procedural2d::WaveFunctionCollapse::Options MapGeneratorsBinding::readCollapse(lua_State* L, int index) {
    haylen::procedural2d::WaveFunctionCollapse::Options options;
    lua::Table::readField(L, index, "width", options.width);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "periodic", options.periodic);
    lua::Table::readField(L, index, "attempts", options.attempts);
    if (lua_getfield(L, index, "fixed") != LUA_TNIL) {
        options.fixed = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, -1);
    }

    lua_pop(L, 1);
    return options;
}

// Rooms and connections count from 1 like Lua lists.
void MapGeneratorsBinding::pushDungeon(lua_State* L, haylen::procedural2d::Dungeon::Result result) {
    lua_createtable(L, 0, 3);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, std::move(result.grid));
    lua_setfield(L, -2, "grid");

    lua_createtable(L, static_cast<int>(result.rooms.size()), 0);
    for (std::size_t index = 0; index < result.rooms.size(); ++index) {
        const haylen::procedural2d::Dungeon::Room& room = result.rooms[index];
        lua_createtable(L, 0, 4);
        lua_pushinteger(L, room.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, room.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, room.width);
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, room.height);
        lua_setfield(L, -2, "height");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, "rooms");

    lua_createtable(L, static_cast<int>(result.connections.size()), 0);
    for (std::size_t index = 0; index < result.connections.size(); ++index) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, static_cast<lua_Integer>(result.connections[index].first) + 1);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(result.connections[index].second) + 1);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, "connections");
}

void MapGeneratorsBinding::pushCollapse(lua_State* L, std::optional<haylen::spatial2d::CellGrid> tiles) {
    if (!tiles) {
        lua_pushnil(L);
        return;
    }

    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, std::move(*tiles));
}

int MapGeneratorsBinding::cellularAutomaton(lua_State* L) {
    const haylen::procedural2d::CellularAutomaton::Options options = readCaves(L, 1);
    haylen::math::Random local(0);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::CellularAutomaton::generate(options, Procedural2DBinding::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsBinding::cellularAutomatonAsync(lua_State* L) {
    const haylen::procedural2d::CellularAutomaton::Options options = readCaves(L, 1);
    haylen::math::Random local(0);
    haylen::math::Random random = Procedural2DBinding::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DBinding::spawn(L, [options, random]() mutable {
        return haylen::procedural2d::CellularAutomaton::generate(options, random);
    }, [](lua_State* state, haylen::spatial2d::CellGrid grid) { lua::Userdata::emplace<haylen::spatial2d::CellGrid>(state, std::move(grid)); });
    // clang-format on
}

// Smooths any grid with `cellularAutomatonStep(grid[, {birthLimit = 5, survivalLimit = 4, solidBorder = true}])` and returns the smoothed grid.
int MapGeneratorsBinding::cellularAutomatonStep(lua_State* L) {
    const auto& grid = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    haylen::procedural2d::CellularAutomaton::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kStepFields});
        lua::Table::readField(L, 2, "birthLimit", options.birthLimit);
        lua::Table::readField(L, 2, "survivalLimit", options.survivalLimit);
        lua::Table::readField(L, 2, "solidBorder", options.solidBorder);
    }

    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::CellularAutomaton::step(grid, options));
    return 1;
}

int MapGeneratorsBinding::drunkardWalk(lua_State* L) {
    const haylen::procedural2d::DrunkardWalk::Options options = readWalk(L, 1);
    haylen::math::Random local(0);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::DrunkardWalk::generate(options, Procedural2DBinding::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsBinding::drunkardWalkAsync(lua_State* L) {
    const haylen::procedural2d::DrunkardWalk::Options options = readWalk(L, 1);
    haylen::math::Random local(0);
    haylen::math::Random random = Procedural2DBinding::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DBinding::spawn(L, [options, random]() mutable {
        return haylen::procedural2d::DrunkardWalk::generate(options, random);
    }, [](lua_State* state, haylen::spatial2d::CellGrid grid) { lua::Userdata::emplace<haylen::spatial2d::CellGrid>(state, std::move(grid)); });
    // clang-format on
}

int MapGeneratorsBinding::dungeon(lua_State* L) {
    const haylen::procedural2d::Dungeon::Options options = readDungeon(L, 1);
    haylen::math::Random local(0);
    pushDungeon(L, haylen::procedural2d::Dungeon::generate(options, Procedural2DBinding::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsBinding::dungeonAsync(lua_State* L) {
    const haylen::procedural2d::Dungeon::Options options = readDungeon(L, 1);
    haylen::math::Random local(0);
    haylen::math::Random random = Procedural2DBinding::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DBinding::spawn(L, [options, random]() mutable {
        return haylen::procedural2d::Dungeon::generate(options, random);
    }, [](lua_State* state, haylen::procedural2d::Dungeon::Result result) { pushDungeon(state, std::move(result)); });
    // clang-format on
}

// Generates a maze with `maze({width, height, algorithm = 'backtracker', seed})`.
int MapGeneratorsBinding::maze(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMazeFields});
    int width = 16;
    int height = 16;
    haylen::procedural2d::Maze::Algorithm algorithm = haylen::procedural2d::Maze::Algorithm::Backtracker;
    lua::Table::readField(L, 1, "width", width);
    lua::Table::readField(L, 1, "height", height);
    lua::Table::readField(L, 1, "algorithm", algorithm);
    haylen::math::Random local(0);
    lua::Userdata::emplace<haylen::procedural2d::Maze>(L, haylen::procedural2d::Maze::generate(width, height, algorithm, Procedural2DBinding::readGenerator(L, 1, local)));
    return 1;
}

// Creates a maze with every wall closed with `newMaze(width, height)`.
int MapGeneratorsBinding::newMaze(lua_State* L) {
    const auto width = lua::Stack::read<int>(L, 1);
    const auto height = lua::Stack::read<int>(L, 2);
    lua::Userdata::emplace<haylen::procedural2d::Maze>(L, width, height);
    return 1;
}

int MapGeneratorsBinding::mazeAsync(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMazeFields});
    int width = 16;
    int height = 16;
    haylen::procedural2d::Maze::Algorithm algorithm = haylen::procedural2d::Maze::Algorithm::Backtracker;
    lua::Table::readField(L, 1, "width", width);
    lua::Table::readField(L, 1, "height", height);
    lua::Table::readField(L, 1, "algorithm", algorithm);
    haylen::math::Random local(0);
    haylen::math::Random random = Procedural2DBinding::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DBinding::spawn(L, [width, height, algorithm, random]() mutable {
        return haylen::procedural2d::Maze::generate(width, height, algorithm, random);
    }, [](lua_State* state, haylen::procedural2d::Maze result) { lua::Userdata::emplace<haylen::procedural2d::Maze>(state, std::move(result)); });
    // clang-format on
}

int MapGeneratorsBinding::waveFunctionCollapse(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kCollapseFields});
    const haylen::procedural2d::WaveFunctionCollapse::Rules rules = readRules(L, 1);
    const haylen::procedural2d::WaveFunctionCollapse::Options options = readCollapse(L, 1);
    haylen::math::Random local(0);
    pushCollapse(L, haylen::procedural2d::WaveFunctionCollapse::generate(rules, options, Procedural2DBinding::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsBinding::waveFunctionCollapseAsync(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kCollapseFields});
    haylen::procedural2d::WaveFunctionCollapse::Rules rules = readRules(L, 1);
    haylen::procedural2d::WaveFunctionCollapse::Options options = readCollapse(L, 1);
    haylen::math::Random local(0);
    haylen::math::Random random = Procedural2DBinding::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DBinding::spawn(L, [rules = std::move(rules), options = std::move(options), random]() mutable {
        return haylen::procedural2d::WaveFunctionCollapse::generate(rules, options, random);
    }, [](lua_State* state, std::optional<haylen::spatial2d::CellGrid> tiles) { pushCollapse(state, std::move(tiles)); });
    // clang-format on
}

// Reads the rules of `waveFunctionCollapse` into an object with `waveFunctionCollapseRules(options)`, to inspect them or check tile maps against them.
int MapGeneratorsBinding::newRules(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kRulesFields});
    lua::Userdata::emplace<haylen::procedural2d::WaveFunctionCollapse::Rules>(L, readRules(L, 1));
    return 1;
}

int MapGeneratorsBinding::rulesAllowed(lua_State* L) {
    const auto& rules = lua::Userdata::check<haylen::procedural2d::WaveFunctionCollapse::Rules>(L, 1);
    const auto first = lua::Stack::read<std::size_t>(L, 2);
    const auto second = lua::Stack::read<std::size_t>(L, 3);
    lua::Stack::push(L, rules.isAllowed(first, second, lua::Stack::read<haylen::procedural2d::WaveFunctionCollapse::Direction>(L, 4)));
    return 1;
}

int MapGeneratorsBinding::rulesWeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::WaveFunctionCollapse::Rules>(L, 1).getWeight(lua::Stack::read<std::size_t>(L, 2)));
    return 1;
}

int MapGeneratorsBinding::rulesTileCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::WaveFunctionCollapse::Rules>(L, 1).getTileCount());
    return 1;
}

// Tells whether every pair of neighbors of a tile map follows the rules with `rules:valid(grid[, periodic])`.
int MapGeneratorsBinding::rulesValid(lua_State* L) {
    const auto& rules = lua::Userdata::check<haylen::procedural2d::WaveFunctionCollapse::Rules>(L, 1);
    const auto& tiles = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 2);
    lua::Stack::push(L, haylen::procedural2d::WaveFunctionCollapse::isValid(tiles, rules, !lua_isnoneornil(L, 3) && lua::Stack::read<bool>(L, 3)));
    return 1;
}

int MapGeneratorsBinding::mazeOpenings(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).getOpenings(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int MapGeneratorsBinding::mazeOpen(lua_State* L) {
    lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).open(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<std::uint8_t>(L, 4));
    return 0;
}

int MapGeneratorsBinding::mazeToGrid(lua_State* L) {
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).toGrid());
    return 1;
}

int MapGeneratorsBinding::mazeWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).getWidth());
    return 1;
}

int MapGeneratorsBinding::mazeHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).getHeight());
    return 1;
}

int MapGeneratorsBinding::mazePassageCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::procedural2d::Maze>(L, 1).getPassageCount());
    return 1;
}

int MapGeneratorsBinding::mask4(lua_State* L) {
    const auto& terrain = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    const haylen::spatial2d::Cell cell{lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)};
    luaL_argcheck(L, terrain.contains(cell), 2, "the cell is outside the grid");
    lua::Stack::push(L, haylen::procedural2d::Autotile::getMask4(terrain, cell, lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4)));
    return 1;
}

int MapGeneratorsBinding::mask8(lua_State* L) {
    const auto& terrain = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    const haylen::spatial2d::Cell cell{lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)};
    luaL_argcheck(L, terrain.contains(cell), 2, "the cell is outside the grid");
    lua::Stack::push(L, haylen::procedural2d::Autotile::getMask8(terrain, cell, lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4)));
    return 1;
}

int MapGeneratorsBinding::blobIndex(lua_State* L) {
    lua::Stack::push(L, haylen::procedural2d::Autotile::getBlobIndex(lua::Stack::read<std::uint8_t>(L, 1)));
    return 1;
}

int MapGeneratorsBinding::autotile4(lua_State* L) {
    const auto& terrain = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::Autotile::apply4(terrain, lua::Stack::read<std::int32_t>(L, 2), lua_isnoneornil(L, 3) || lua::Stack::read<bool>(L, 3)));
    return 1;
}

int MapGeneratorsBinding::autotile8(lua_State* L) {
    const auto& terrain = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::Autotile::apply8(terrain, lua::Stack::read<std::int32_t>(L, 2), lua_isnoneornil(L, 3) || lua::Stack::read<bool>(L, 3)));
    return 1;
}

// Picks Wang tiles with `autotileWang(colors, wangSet, seed)`, where `wangSet` is a table of `haylen.tiled` tilesets with `kind` and `tiles = {{tileId, wangId}}`.
int MapGeneratorsBinding::autotileWang(lua_State* L) {
    const auto& colors = lua::Userdata::check<haylen::spatial2d::CellGrid>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const auto seed = static_cast<std::uint64_t>(luaL_optinteger(L, 3, 0));
    haylen::tiled::WangSet set;
    lua::Table::readField(L, 2, "kind", set.kind);
    lua_getfield(L, 2, "tiles");
    lua::Table::requireType(L, "tiles", LUA_TTABLE);
    const lua_Integer count = luaL_len(L, -1);
    for (lua_Integer tile = 1; tile <= count; ++tile) {
        lua_rawgeti(L, -1, tile);
        haylen::tiled::WangSet::Tile& entry = set.tiles.emplace_back();
        lua::Table::readField(L, lua_gettop(L), "tileId", entry.tileId);
        std::vector<std::uint8_t> wangId;
        lua::Table::readField(L, lua_gettop(L), "wangId", wangId);
        std::copy_n(wangId.begin(), std::min(wangId.size(), entry.wangId.size()), entry.wangId.begin());
        lua_pop(L, 1);
    }

    lua::Userdata::emplace<haylen::spatial2d::CellGrid>(L, haylen::procedural2d::Autotile::applyWang(colors, set, seed));
    return 1;
}

void MapGeneratorsBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::procedural2d::Maze>(L).function("openings", &lua::Binding::native<&mazeOpenings>).function("open", &lua::Binding::native<&mazeOpen>).function("toGrid", &lua::Binding::native<&mazeToGrid>).property("width", &mazeWidth).property("height", &mazeHeight).property("passageCount", &mazePassageCount).install();
    lua::ClassBuilder<haylen::procedural2d::WaveFunctionCollapse::Rules>(L).function("allowed", &lua::Binding::native<&rulesAllowed>).function("weight", &lua::Binding::native<&rulesWeight>).function("valid", &lua::Binding::native<&rulesValid>).property("tileCount", &rulesTileCount).install();
}

void MapGeneratorsBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"cellularAutomaton", &lua::Binding::native<&cellularAutomaton>}, {"cellularAutomatonAsync", &lua::Binding::native<&cellularAutomatonAsync>}, {"cellularAutomatonStep", &lua::Binding::native<&cellularAutomatonStep>}, {"drunkardWalk", &lua::Binding::native<&drunkardWalk>}, {"drunkardWalkAsync", &lua::Binding::native<&drunkardWalkAsync>}, {"dungeon", &lua::Binding::native<&dungeon>}, {"dungeonAsync", &lua::Binding::native<&dungeonAsync>}, {"maze", &lua::Binding::native<&maze>}, {"mazeAsync", &lua::Binding::native<&mazeAsync>}, {"newMaze", &lua::Binding::native<&newMaze>}, {"waveFunctionCollapse", &lua::Binding::native<&waveFunctionCollapse>}, {"waveFunctionCollapseAsync", &lua::Binding::native<&waveFunctionCollapseAsync>}, {"waveFunctionCollapseRules", &lua::Binding::native<&newRules>}, {"mask4", &lua::Binding::native<&mask4>}, {"mask8", &lua::Binding::native<&mask8>}, {"blobIndex", &lua::Binding::native<&blobIndex>}, {"autotile4", &lua::Binding::native<&autotile4>}, {"autotile8", &lua::Binding::native<&autotile8>}, {"autotileWang", &lua::Binding::native<&autotileWang>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
    for (const auto& [name, side] : {std::pair{"north", haylen::procedural2d::Maze::kNorth}, std::pair{"east", haylen::procedural2d::Maze::kEast}, std::pair{"south", haylen::procedural2d::Maze::kSouth}, std::pair{"west", haylen::procedural2d::Maze::kWest}}) {
        lua_pushinteger(L, side);
        lua_setfield(L, -2, name);
    }
}

} // namespace haylen::lua::procedural2d
