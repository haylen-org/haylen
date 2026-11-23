#include "2d/physics/DestructionBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "haylen/2d/physics/Fracture.hpp"
#include "haylen/2d/physics/Terrain.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "math/PolygonBinding.hpp"

namespace haylen::lua {

template <> struct Type<lua::physics2d::ScriptedOwner<haylen::physics2d::Terrain>> {
    static constexpr const char* name = "haylen.Terrain";
    using Storage = lua::physics2d::ScriptedOwner<haylen::physics2d::Terrain>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::physics2d::Explosion::Falloff> {
    static std::optional<haylen::physics2d::Explosion::Falloff> fromName(std::string_view name) {
        return haylen::physics2d::Explosion::falloffFromName(name);
    }

    static std::string_view name(haylen::physics2d::Explosion::Falloff value) {
        return haylen::physics2d::Explosion::falloffName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::physics2d {

haylen::physics2d::Explosion::Options DestructionBinding::readExplosion(lua_State* L, int index, haylen::physics2d::Explosion::Options options) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kExplosionFields});
    lua::Table::readField(L, index, "x", options.center.x);
    lua::Table::readField(L, index, "y", options.center.y);
    lua::Table::readField(L, index, "radius", options.radius);
    lua::Table::readField(L, index, "impulse", options.impulse);
    lua::Table::readField(L, index, "falloff", options.falloff);
    lua::Table::readField(L, index, "occlusion", options.occlusion);
    options.filter = ShapeBinding::readFilter(L, index, options.filter);
    return options;
}

void DestructionBinding::pushHits(lua_State* L, int worldIndex, const std::vector<haylen::physics2d::Explosion::Hit>& hits) {
    const int world = lua_absindex(L, worldIndex);
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (std::size_t index = 0; index < hits.size(); ++index) {
        lua_createtable(L, 0, 5);
        Physics2DBinding::push(L, world, hits[index].body);
        lua_setfield(L, -2, "body");
        lua::Stack::push(L, hits[index].point.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, hits[index].point.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, hits[index].impulse.x);
        lua_setfield(L, -2, "impulseX");
        lua::Stack::push(L, hits[index].impulse.y);
        lua_setfield(L, -2, "impulseY");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

// Creates a terrain with `newTerrain(world, {columns, rows, cellSize, x, y, chunkSize, simplifyTolerance, friction, restitution, category, mask, group})`. Its user value is the world object, which its bodies belong to.
int DestructionBinding::newTerrain(lua_State* L) {
    const std::shared_ptr<haylen::physics2d::World>& world = lua::Userdata::checkShared<haylen::physics2d::World>(L, 1);
    haylen::physics2d::Terrain::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kTerrainFields});
        lua::Table::readField(L, 2, "columns", options.columns);
        lua::Table::readField(L, 2, "rows", options.rows);
        lua::Table::readField(L, 2, "cellSize", options.cellSize);
        lua::Table::readField(L, 2, "x", options.origin.x);
        lua::Table::readField(L, 2, "y", options.origin.y);
        lua::Table::readField(L, 2, "chunkSize", options.chunkSize);
        lua::Table::readField(L, 2, "simplifyTolerance", options.simplifyTolerance);
        lua::Table::readField(L, 2, "friction", options.shape.friction);
        lua::Table::readField(L, 2, "restitution", options.shape.restitution);
        options.shape.filter = ShapeBinding::readFilter(L, 2, {});
    }

    lua::Userdata::emplace<ScriptedOwner<haylen::physics2d::Terrain>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

// Sets every sample with `terrain.samples = values`, from a list of values from 0 to 1 stored row by row or from a function of the column and row that returns them.
int DestructionBinding::terrainSetSamples(lua_State* L) {
    haylen::physics2d::Terrain& terrain = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object;
    const auto columns = static_cast<std::size_t>(terrain.getColumns());
    const bool computed = lua_isfunction(L, 3);
    if (!computed) {
        luaL_checktype(L, 3, LUA_TTABLE);
    }

    const std::size_t count = computed ? columns * static_cast<std::size_t>(terrain.getRows()) : static_cast<std::size_t>(lua_rawlen(L, 3));
    std::vector<std::uint8_t> values(count);
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (computed) {
            lua_pushvalue(L, 3);
            lua_pushinteger(L, static_cast<lua_Integer>(index % columns));
            lua_pushinteger(L, static_cast<lua_Integer>(index / columns));
            lua_call(L, 2, 1);
        } else {
            lua_rawgeti(L, 3, static_cast<lua_Integer>(index + 1));
        }

        const double value = luaL_checknumber(L, -1);
        values[index] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
        lua_pop(L, 1);
    }

    terrain.setSamples(values);
    return 0;
}

int DestructionBinding::terrainSample(lua_State* L) {
    const haylen::physics2d::Terrain& terrain = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object;
    lua::Stack::push(L, static_cast<float>(terrain.getSample(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3))) / 255.0F);
    return 1;
}

int DestructionBinding::terrainGetSamples(lua_State* L) {
    const std::span<const std::uint8_t> samples = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getSamples();
    lua_createtable(L, static_cast<int>(samples.size()), 0);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        lua_pushnumber(L, static_cast<lua_Number>(samples[index]) / 255.0);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int DestructionBinding::terrainIsSolid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.isSolid(lua::Stack::read<haylen::math::Vec2>(L, 2)));
    return 1;
}

int DestructionBinding::terrainFill(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.fill(haylen::math::Circle{{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4)});
    return 0;
}

int DestructionBinding::terrainCarve(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.carve(haylen::math::Circle{{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4)});
    return 0;
}

int DestructionBinding::terrainFillPolygon(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.fill(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2));
    return 0;
}

int DestructionBinding::terrainCarvePolygon(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.carve(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2));
    return 0;
}

// Carves a crater with `explode(x, y, radius, blast)` and returns the hits of the blast, whose options default to the center of the crater and twice its radius.
int DestructionBinding::terrainExplode(lua_State* L) {
    haylen::physics2d::Terrain& terrain = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object;
    const haylen::math::Circle crater{{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4)};
    haylen::physics2d::Explosion::Options blast{.center = crater.center, .radius = crater.radius * 2.0F};
    if (!lua_isnoneornil(L, 5)) {
        blast = readExplosion(L, 5, blast);
    }

    const std::vector<haylen::physics2d::Explosion::Hit> hits = terrain.explode(crater, blast);
    lua_getiuservalue(L, 1, 1);
    pushHits(L, -1, hits);
    return 1;
}

int DestructionBinding::terrainUpdate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.update());
    return 1;
}

int DestructionBinding::terrainOutlines(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getOutlines());
    return 1;
}

int DestructionBinding::terrainBodies(lua_State* L) {
    const std::vector<haylen::physics2d::Body> bodies = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getBodies();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::pushList(L, -1, bodies);
    return 1;
}

int DestructionBinding::terrainColumns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getColumns());
    return 1;
}

int DestructionBinding::terrainRows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getRows());
    return 1;
}

int DestructionBinding::terrainCellSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getCellSize());
    return 1;
}

int DestructionBinding::terrainBounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getBounds());
    return 1;
}

int DestructionBinding::terrainChunkCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getChunkCount());
    return 1;
}

int DestructionBinding::terrainDirtyChunkCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Terrain>>(L, 1).object.getDirtyChunkCount());
    return 1;
}

// Pushes bodies away with `explode(world, {x, y, radius, impulse, falloff, occlusion, category, mask})` and returns the hits.
int DestructionBinding::explode(lua_State* L) {
    haylen::physics2d::World& world = lua::Userdata::check<haylen::physics2d::World>(L, 1);
    pushHits(L, 1, haylen::physics2d::Explosion::apply(world, readExplosion(L, 2, {})));
    return 1;
}

// Breaks a body with `fracture(body, {pieces, impact, seed, minimumArea})` and returns its pieces.
int DestructionBinding::fracture(lua_State* L) {
    const haylen::physics2d::Body body = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1).handle;
    haylen::physics2d::Fracture::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kFractureFields});
        lua::Table::readField(L, 2, "pieces", options.pieces);
        lua::Table::readField(L, 2, "impact", options.impact);
        lua::Table::readField(L, 2, "seed", options.seed);
        lua::Table::readField(L, 2, "minimumArea", options.minimumArea);
    }

    const std::vector<haylen::physics2d::Body> pieces = haylen::physics2d::Fracture::shatter(body, options);
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::pushList(L, -1, pieces);
    return 1;
}

int DestructionBinding::splitPolygon(lua_State* L) {
    haylen::physics2d::Fracture::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kFractureFields});
        lua::Table::readField(L, 2, "pieces", options.pieces);
        lua::Table::readField(L, 2, "impact", options.impact);
        lua::Table::readField(L, 2, "seed", options.seed);
        lua::Table::readField(L, 2, "minimumArea", options.minimumArea);
    }

    lua::Stack::push(L, haylen::physics2d::Fracture::split(lua::math::PolygonBinding::readShape(L, 1), options));
    return 1;
}

void DestructionBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<haylen::physics2d::Terrain>>(L).function("sample", &lua::Binding::native<&terrainSample>).function("isSolid", &lua::Binding::native<&terrainIsSolid>).function("fill", &lua::Binding::native<&terrainFill>).function("carve", &lua::Binding::native<&terrainCarve>).function("fillPolygon", &lua::Binding::native<&terrainFillPolygon>).function("carvePolygon", &lua::Binding::native<&terrainCarvePolygon>).function("explode", &lua::Binding::native<&terrainExplode>).function("update", &lua::Binding::native<&terrainUpdate>).function("outlines", &lua::Binding::native<&terrainOutlines>).function("bodies", &lua::Binding::native<&terrainBodies>).property("samples", &lua::Binding::native<&terrainGetSamples>, &lua::Binding::native<&terrainSetSamples>).property("columns", &terrainColumns).property("rows", &terrainRows).property("cellSize", &terrainCellSize).property("bounds", &terrainBounds).property("chunkCount", &terrainChunkCount).property("dirtyChunkCount", &terrainDirtyChunkCount).install();
}

void DestructionBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTerrain", &lua::Binding::native<&newTerrain>}, {"explode", &lua::Binding::native<&explode>}, {"fracture", &lua::Binding::native<&fracture>}, {"splitPolygon", &lua::Binding::native<&splitPolygon>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::physics2d
