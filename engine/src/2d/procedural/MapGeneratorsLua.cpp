#include "2d/procedural/MapGeneratorsLua.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "2d/procedural/Procedural2DLua.hpp"
#include "2d/spatial/GridLua.hpp"
#include "haylen/2d/procedural/Autotile.hpp"
#include "haylen/2d/tiled/WangSet.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<procedural2d::Maze> {
    static constexpr const char* name = "haylen.Maze";
    using Storage = procedural2d::Maze;
};

template <> struct EnumNames<procedural2d::Dungeon::Method> {
    static std::optional<procedural2d::Dungeon::Method> fromName(std::string_view name) {
        if (name == "bsp") {
            return procedural2d::Dungeon::Method::Bsp;
        }
        if (name == "placement") {
            return procedural2d::Dungeon::Method::Placement;
        }
        return std::nullopt;
    }
    static std::string_view name(procedural2d::Dungeon::Method value) {
        return value == procedural2d::Dungeon::Method::Bsp ? "bsp" : "placement";
    }
};

template <> struct EnumNames<procedural2d::Maze::Algorithm> {
    static std::optional<procedural2d::Maze::Algorithm> fromName(std::string_view name) {
        return procedural2d::Maze::algorithmFromName(name);
    }
    static std::string_view name(procedural2d::Maze::Algorithm value) {
        switch (value) {
        case procedural2d::Maze::Algorithm::Prim:
            return "prim";
        case procedural2d::Maze::Algorithm::Kruskal:
            return "kruskal";
        case procedural2d::Maze::Algorithm::Backtracker:
            break;
        }
        return "backtracker";
    }
};

template <> struct EnumNames<procedural2d::WaveFunctionCollapse::Direction> {
    static std::optional<procedural2d::WaveFunctionCollapse::Direction> fromName(std::string_view name) {
        using Direction = procedural2d::WaveFunctionCollapse::Direction;
        if (name == "right") {
            return Direction::Right;
        }
        if (name == "down") {
            return Direction::Down;
        }
        if (name == "left") {
            return Direction::Left;
        }
        if (name == "up") {
            return Direction::Up;
        }
        return std::nullopt;
    }
    static std::string_view name(procedural2d::WaveFunctionCollapse::Direction value) {
        using Direction = procedural2d::WaveFunctionCollapse::Direction;
        return value == Direction::Right ? "right" : (value == Direction::Down ? "down" : (value == Direction::Left ? "left" : "up"));
    }
};

} // namespace haylen::lua

namespace haylen::procedural2d {

CellularAutomaton::Options MapGeneratorsLua::readCaves(lua_State* L, int index) {
    CellularAutomaton::Options options;
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

DrunkardWalk::Options MapGeneratorsLua::readWalk(lua_State* L, int index) {
    DrunkardWalk::Options options;
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

Dungeon::Options MapGeneratorsLua::readDungeon(lua_State* L, int index) {
    Dungeon::Options options;
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

// Rules come from a sample grid, or from a tile count with allow = {{first, second, 'right'}, ...}. Weights list the tiles from tile 0 on.
WaveFunctionCollapse::Rules MapGeneratorsLua::readRules(lua_State* L, int index) {
    std::optional<WaveFunctionCollapse::Rules> rules;
    if (lua_getfield(L, index, "sample") != LUA_TNIL) {
        bool periodic = false;
        lua::Table::readField(L, index, "periodicSample", periodic);
        rules.emplace(WaveFunctionCollapse::Rules::fromSample(lua::Userdata::check<spatial2d::CellGrid>(L, -1), periodic));
    } else {
        std::size_t tiles = 0;
        lua::Table::readField(L, index, "tiles", tiles);
        rules.emplace(tiles);
        if (lua_getfield(L, index, "allow") != LUA_TNIL) {
            luaL_checktype(L, -1, LUA_TTABLE);
            const lua_Integer count = luaL_len(L, -1);
            for (lua_Integer pair = 1; pair <= count; ++pair) {
                lua_rawgeti(L, -1, pair);
                luaL_checktype(L, -1, LUA_TTABLE);
                lua_rawgeti(L, -1, 1);
                lua_rawgeti(L, -2, 2);
                lua_rawgeti(L, -3, 3);
                rules->allow(lua::Stack::read<std::size_t>(L, -3), lua::Stack::read<std::size_t>(L, -2), lua::Stack::read<WaveFunctionCollapse::Direction>(L, -1));
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

WaveFunctionCollapse::Options MapGeneratorsLua::readCollapse(lua_State* L, int index) {
    WaveFunctionCollapse::Options options;
    lua::Table::readField(L, index, "width", options.width);
    lua::Table::readField(L, index, "height", options.height);
    lua::Table::readField(L, index, "periodic", options.periodic);
    lua::Table::readField(L, index, "attempts", options.attempts);
    if (lua_getfield(L, index, "fixed") != LUA_TNIL) {
        options.fixed = lua::Userdata::check<spatial2d::CellGrid>(L, -1);
    }
    lua_pop(L, 1);
    return options;
}

// Rooms and connections count from 1 like Lua lists.
void MapGeneratorsLua::pushDungeon(lua_State* L, Dungeon::Result dungeon) {
    lua_createtable(L, 0, 3);
    lua::Userdata::emplace<spatial2d::CellGrid>(L, std::move(dungeon.grid));
    lua_setfield(L, -2, "grid");

    lua_createtable(L, static_cast<int>(dungeon.rooms.size()), 0);
    for (std::size_t index = 0; index < dungeon.rooms.size(); ++index) {
        const Dungeon::Room& room = dungeon.rooms[index];
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

    lua_createtable(L, static_cast<int>(dungeon.connections.size()), 0);
    for (std::size_t index = 0; index < dungeon.connections.size(); ++index) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, static_cast<lua_Integer>(dungeon.connections[index].first) + 1);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(dungeon.connections[index].second) + 1);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "connections");
}

void MapGeneratorsLua::pushCollapse(lua_State* L, std::optional<spatial2d::CellGrid> tiles) {
    if (!tiles) {
        lua_pushnil(L);
        return;
    }
    lua::Userdata::emplace<spatial2d::CellGrid>(L, std::move(*tiles));
}

int MapGeneratorsLua::caves(lua_State* L) {
    const CellularAutomaton::Options options = readCaves(L, 1);
    math::Random local(0);
    lua::Userdata::emplace<spatial2d::CellGrid>(L, CellularAutomaton::generate(options, Procedural2DLua::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsLua::cavesAsync(lua_State* L) {
    const CellularAutomaton::Options options = readCaves(L, 1);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [options, random]() mutable {
        return CellularAutomaton::generate(options, random);
    }, [](lua_State* state, spatial2d::CellGrid grid) { lua::Userdata::emplace<spatial2d::CellGrid>(state, std::move(grid)); });
    // clang-format on
}

int MapGeneratorsLua::drunkardWalk(lua_State* L) {
    const DrunkardWalk::Options options = readWalk(L, 1);
    math::Random local(0);
    lua::Userdata::emplace<spatial2d::CellGrid>(L, DrunkardWalk::generate(options, Procedural2DLua::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsLua::drunkardWalkAsync(lua_State* L) {
    const DrunkardWalk::Options options = readWalk(L, 1);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [options, random]() mutable {
        return DrunkardWalk::generate(options, random);
    }, [](lua_State* state, spatial2d::CellGrid grid) { lua::Userdata::emplace<spatial2d::CellGrid>(state, std::move(grid)); });
    // clang-format on
}

int MapGeneratorsLua::dungeon(lua_State* L) {
    const Dungeon::Options options = readDungeon(L, 1);
    math::Random local(0);
    pushDungeon(L, Dungeon::generate(options, Procedural2DLua::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsLua::dungeonAsync(lua_State* L) {
    const Dungeon::Options options = readDungeon(L, 1);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [options, random]() mutable {
        return Dungeon::generate(options, random);
    }, [](lua_State* state, Dungeon::Result result) { pushDungeon(state, std::move(result)); });
    // clang-format on
}

// Generates a maze with maze({width, height, algorithm = 'backtracker', seed}).
int MapGeneratorsLua::maze(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMazeFields});
    int width = 16;
    int height = 16;
    Maze::Algorithm algorithm = Maze::Algorithm::Backtracker;
    lua::Table::readField(L, 1, "width", width);
    lua::Table::readField(L, 1, "height", height);
    lua::Table::readField(L, 1, "algorithm", algorithm);
    math::Random local(0);
    lua::Userdata::emplace<Maze>(L, Maze::generate(width, height, algorithm, Procedural2DLua::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsLua::mazeAsync(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMazeFields});
    int width = 16;
    int height = 16;
    Maze::Algorithm algorithm = Maze::Algorithm::Backtracker;
    lua::Table::readField(L, 1, "width", width);
    lua::Table::readField(L, 1, "height", height);
    lua::Table::readField(L, 1, "algorithm", algorithm);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [width, height, algorithm, random]() mutable {
        return Maze::generate(width, height, algorithm, random);
    }, [](lua_State* state, Maze result) { lua::Userdata::emplace<Maze>(state, std::move(result)); });
    // clang-format on
}

int MapGeneratorsLua::waveFunctionCollapse(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kCollapseFields});
    const WaveFunctionCollapse::Rules rules = readRules(L, 1);
    const WaveFunctionCollapse::Options options = readCollapse(L, 1);
    math::Random local(0);
    pushCollapse(L, WaveFunctionCollapse::generate(rules, options, Procedural2DLua::readGenerator(L, 1, local)));
    return 1;
}

int MapGeneratorsLua::waveFunctionCollapseAsync(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kCollapseFields});
    WaveFunctionCollapse::Rules rules = readRules(L, 1);
    WaveFunctionCollapse::Options options = readCollapse(L, 1);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [rules = std::move(rules), options = std::move(options), random]() mutable {
        return WaveFunctionCollapse::generate(rules, options, random);
    }, [](lua_State* state, std::optional<spatial2d::CellGrid> tiles) { pushCollapse(state, std::move(tiles)); });
    // clang-format on
}

int MapGeneratorsLua::mazeOpenings(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Maze>(L, 1).getOpenings(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int MapGeneratorsLua::mazeOpen(lua_State* L) {
    lua::Userdata::check<Maze>(L, 1).open(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<std::uint8_t>(L, 4));
    return 0;
}

int MapGeneratorsLua::mazeToGrid(lua_State* L) {
    lua::Userdata::emplace<spatial2d::CellGrid>(L, lua::Userdata::check<Maze>(L, 1).toGrid());
    return 1;
}

int MapGeneratorsLua::mazeWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Maze>(L, 1).getWidth());
    return 1;
}

int MapGeneratorsLua::mazeHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Maze>(L, 1).getHeight());
    return 1;
}

int MapGeneratorsLua::mazePassageCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Maze>(L, 1).getPassageCount());
    return 1;
}

int MapGeneratorsLua::mask4(lua_State* L) {
    const auto& terrain = lua::Userdata::check<spatial2d::CellGrid>(L, 1);
    const spatial2d::Cell cell{lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)};
    luaL_argcheck(L, terrain.contains(cell), 2, "the cell is outside the grid");
    lua::Stack::push(L, Autotile::getMask4(terrain, cell, lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4)));
    return 1;
}

int MapGeneratorsLua::mask8(lua_State* L) {
    const auto& terrain = lua::Userdata::check<spatial2d::CellGrid>(L, 1);
    const spatial2d::Cell cell{lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)};
    luaL_argcheck(L, terrain.contains(cell), 2, "the cell is outside the grid");
    lua::Stack::push(L, Autotile::getMask8(terrain, cell, lua_isnoneornil(L, 4) || lua::Stack::read<bool>(L, 4)));
    return 1;
}

int MapGeneratorsLua::blobIndex(lua_State* L) {
    lua::Stack::push(L, Autotile::getBlobIndex(lua::Stack::read<std::uint8_t>(L, 1)));
    return 1;
}

int MapGeneratorsLua::autotile4(lua_State* L) {
    const auto& terrain = lua::Userdata::check<spatial2d::CellGrid>(L, 1);
    lua::Userdata::emplace<spatial2d::CellGrid>(L, Autotile::apply4(terrain, lua::Stack::read<std::int32_t>(L, 2), lua_isnoneornil(L, 3) || lua::Stack::read<bool>(L, 3)));
    return 1;
}

int MapGeneratorsLua::autotile8(lua_State* L) {
    const auto& terrain = lua::Userdata::check<spatial2d::CellGrid>(L, 1);
    lua::Userdata::emplace<spatial2d::CellGrid>(L, Autotile::apply8(terrain, lua::Stack::read<std::int32_t>(L, 2), lua_isnoneornil(L, 3) || lua::Stack::read<bool>(L, 3)));
    return 1;
}

// Picks Wang tiles with wang(colors, wangSet, seed), where wangSet is a table of haylen.tiled tilesets with kind and tiles = {{tileId, wangId}}.
int MapGeneratorsLua::wang(lua_State* L) {
    const auto& colors = lua::Userdata::check<spatial2d::CellGrid>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const auto seed = static_cast<std::uint64_t>(luaL_optinteger(L, 3, 0));
    tiled::WangSet set;
    lua::Table::readField(L, 2, "kind", set.kind);
    lua_getfield(L, 2, "tiles");
    luaL_checktype(L, -1, LUA_TTABLE);
    const lua_Integer count = luaL_len(L, -1);
    for (lua_Integer tile = 1; tile <= count; ++tile) {
        lua_rawgeti(L, -1, tile);
        tiled::WangSet::Tile& entry = set.tiles.emplace_back();
        lua::Table::readField(L, lua_gettop(L), "tileId", entry.tileId);
        std::vector<std::uint8_t> wangId;
        lua::Table::readField(L, lua_gettop(L), "wangId", wangId);
        std::copy_n(wangId.begin(), std::min(wangId.size(), entry.wangId.size()), entry.wangId.begin());
        lua_pop(L, 1);
    }
    lua::Userdata::emplace<spatial2d::CellGrid>(L, Autotile::applyWang(colors, set, seed));
    return 1;
}

void MapGeneratorsLua::install(lua_State* L) {
    lua::ClassBuilder<Maze>(L).function("openings", &lua::Binding::native<&mazeOpenings>).function("open", &lua::Binding::native<&mazeOpen>).function("toGrid", &lua::Binding::native<&mazeToGrid>).property("width", &mazeWidth).property("height", &mazeHeight).property("passageCount", &mazePassageCount).install();
}

void MapGeneratorsLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"caves", &lua::Binding::native<&caves>}, {"cavesAsync", &lua::Binding::native<&cavesAsync>}, {"drunkardWalk", &lua::Binding::native<&drunkardWalk>}, {"drunkardWalkAsync", &lua::Binding::native<&drunkardWalkAsync>}, {"dungeon", &lua::Binding::native<&dungeon>}, {"dungeonAsync", &lua::Binding::native<&dungeonAsync>}, {"maze", &lua::Binding::native<&maze>}, {"mazeAsync", &lua::Binding::native<&mazeAsync>}, {"waveFunctionCollapse", &lua::Binding::native<&waveFunctionCollapse>}, {"waveFunctionCollapseAsync", &lua::Binding::native<&waveFunctionCollapseAsync>}, {"mask4", &lua::Binding::native<&mask4>}, {"mask8", &lua::Binding::native<&mask8>}, {"blobIndex", &lua::Binding::native<&blobIndex>}, {"autotile4", &lua::Binding::native<&autotile4>}, {"autotile8", &lua::Binding::native<&autotile8>}, {"wang", &lua::Binding::native<&wang>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
    for (const auto& [name, side] : {std::pair{"north", Maze::kNorth}, std::pair{"east", Maze::kEast}, std::pair{"south", Maze::kSouth}, std::pair{"west", Maze::kWest}}) {
        lua_pushinteger(L, side);
        lua_setfield(L, -2, name);
    }
}

} // namespace haylen::procedural2d
