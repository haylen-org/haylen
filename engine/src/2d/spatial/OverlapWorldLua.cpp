#include "2d/spatial/OverlapWorldLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::spatial2d {

ScriptedOverlapWorld& OverlapWorldLua::check(lua_State* L) {
    return lua::Userdata::check<ScriptedOverlapWorld>(L, 1);
}

std::uint32_t OverlapWorldLua::readId(lua_State* L, int index) {
    const lua_Integer id = luaL_checkinteger(L, index);
    luaL_argcheck(L, id >= 0 && id <= static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max()), index, "an id is an integer of 32 bits");
    return static_cast<std::uint32_t>(id);
}

// Reads the bits of a box at the top of the stack, which are integers of 32 bits.
std::uint32_t OverlapWorldLua::readMask(lua_State* L, const char* field, std::uint32_t fallback) {
    if (lua_getfield(L, -1, field) == LUA_TNIL) {
        lua_pop(L, 1);
        return fallback;
    }
    int isInteger = 0;
    const lua_Integer bits = lua_tointegerx(L, -1, &isInteger);
    lua_pop(L, 1);
    if (isInteger == 0 || bits < 0 || bits > static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max())) {
        luaL_error(L, "The \"%s\" of a box is an integer of 32 bits.", field);
    }
    return static_cast<std::uint32_t>(bits);
}

// Creates a world with `newOverlapWorld({kinds = {'body', 'hit'}, touching = true, cellSize = 0})`.
int OverlapWorldLua::newOverlapWorld(lua_State* L) {
    OverlapWorld::Options options;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kOptions});
        lua::Table::readField(L, 1, "kinds", options.kinds);
        lua::Table::readField(L, 1, "touching", options.touching);
        lua::Table::readField(L, 1, "cellSize", options.cellSize);
    }
    lua::Userdata::emplace<ScriptedOverlapWorld>(L, ScriptedOverlapWorld{.world = OverlapWorld(std::move(options))});
    return 1;
}

// Defines a set with a list of `{kind, x, y, width, height, layers = 1, mask = 0, depth = 0}` and returns its id.
int OverlapWorldLua::defineSet(lua_State* L) {
    OverlapWorld& world = check(L).world;
    luaL_checktype(L, 2, LUA_TTABLE);
    const std::vector<std::string>& kinds = world.getOptions().kinds;
    std::vector<OverlapWorld::Box> boxes;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, 2));
    for (lua_Integer entry = 1; entry <= count; ++entry) {
        lua_rawgeti(L, 2, entry);
        luaL_checktype(L, -1, LUA_TTABLE);
        lua::Table::checkFields(L, -1, {kBoxFields});
        OverlapWorld::Box box;
        lua::Table::readField(L, -1, "x", box.rect.x);
        lua::Table::readField(L, -1, "y", box.rect.y);
        lua::Table::readField(L, -1, "width", box.rect.width);
        lua::Table::readField(L, -1, "height", box.rect.height);
        lua::Table::readField(L, -1, "depth", box.depth);
        box.layers = readMask(L, "layers", 1);
        box.mask = readMask(L, "mask", 0);
        std::string kind;
        lua::Table::readField(L, -1, "kind", kind);
        if (!kind.empty()) {
            std::size_t found = 0;
            while (found < kinds.size() && kinds[found] != kind) {
                ++found;
            }
            if (found == kinds.size()) {
                return luaL_error(L, "The box kind \"%s\" is not one of the kinds of the overlap world.", kind.c_str());
            }
            box.kind = static_cast<std::uint8_t>(found);
        }
        boxes.push_back(box);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, static_cast<lua_Integer>(world.defineSet(boxes)));
    return 1;
}

int OverlapWorldLua::add(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).world.add()));
    return 1;
}

// Places an entity with `place(entity, x, y, depth, facing, set)`, where a negative facing mirrors its boxes and the set 0 hides them.
int OverlapWorldLua::place(lua_State* L) {
    check(L).world.place(readId(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lua::Stack::read<float>(L, 5), luaL_checknumber(L, 6) < 0.0, readId(L, 7));
    return 0;
}

// Places entities from groups of six numbers of a float buffer, `entity, x, y, depth, facing, set`, from the group at the position `first`, 1 by default, to the end of the buffer.
int OverlapWorldLua::placeAll(lua_State* L) {
    OverlapWorld& world = check(L).world;
    const std::span<const float> values = lua::Userdata::check<core::FloatBuffer>(L, 2).getValues();
    const lua_Integer first = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, first >= 1, 3, "the first value of a buffer is at position 1");
    for (std::size_t offset = static_cast<std::size_t>(first - 1); offset + kPlacementSize <= values.size(); offset += kPlacementSize) {
        const std::span<const float> group = values.subspan(offset, kPlacementSize);
        world.place(static_cast<std::uint32_t>(group[0]), {group[1], group[2]}, group[3], group[4] < 0.0F, static_cast<std::uint32_t>(group[5]));
    }
    return 0;
}

int OverlapWorldLua::remove(lua_State* L) {
    check(L).world.remove(readId(L, 2));
    return 0;
}

// Writes groups of eight numbers into a float buffer, `source, sourceBox, target, targetBox, x, y, width, height` of the overlap, as many as fit, and returns the count of every contact, or returns a list of tables without a buffer.
int OverlapWorldLua::contacts(lua_State* L) {
    ScriptedOverlapWorld& scripted = check(L);
    scripted.world.collect(scripted.contacts);
    if (lua_isnoneornil(L, 2)) {
        lua_createtable(L, static_cast<int>(scripted.contacts.size()), 0);
        for (std::size_t index = 0; index < scripted.contacts.size(); ++index) {
            const OverlapWorld::Contact& contact = scripted.contacts[index];
            lua_createtable(L, 0, 5);
            lua_pushinteger(L, contact.source);
            lua_setfield(L, -2, "source");
            lua_pushinteger(L, contact.sourceBox);
            lua_setfield(L, -2, "sourceBox");
            lua_pushinteger(L, contact.target);
            lua_setfield(L, -2, "target");
            lua_pushinteger(L, contact.targetBox);
            lua_setfield(L, -2, "targetBox");
            lua::Stack::push(L, contact.overlap);
            lua_setfield(L, -2, "overlap");
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
        return 1;
    }

    const std::span<float> values = lua::Userdata::check<core::FloatBuffer>(L, 2).getValues();
    const std::size_t fitting = std::min(scripted.contacts.size(), values.size() / kContactSize);
    for (std::size_t index = 0; index < fitting; ++index) {
        const OverlapWorld::Contact& contact = scripted.contacts[index];
        const std::span<float> group = values.subspan(index * kContactSize, kContactSize);
        group[0] = static_cast<float>(contact.source);
        group[1] = static_cast<float>(contact.sourceBox);
        group[2] = static_cast<float>(contact.target);
        group[3] = static_cast<float>(contact.targetBox);
        group[4] = contact.overlap.x;
        group[5] = contact.overlap.y;
        group[6] = contact.overlap.width;
        group[7] = contact.overlap.height;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(scripted.contacts.size()));
    return 1;
}

int OverlapWorldLua::rest(lua_State* L) {
    check(L).world.rest(readId(L, 2), readId(L, 3), readId(L, 4));
    return 0;
}

int OverlapWorldLua::resting(lua_State* L) {
    lua::Stack::push(L, check(L).world.isResting(readId(L, 2), readId(L, 3)));
    return 1;
}

int OverlapWorldLua::step(lua_State* L) {
    check(L).world.step();
    return 0;
}

int OverlapWorldLua::clear(lua_State* L) {
    check(L).world.clear();
    return 0;
}

int OverlapWorldLua::debugDraw(lua_State* L) {
    check(L).world.debugDraw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int OverlapWorldLua::entityCount(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).world.getEntityCount()));
    return 1;
}

void OverlapWorldLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOverlapWorld>(L).function("defineSet", &lua::Binding::native<&defineSet>).function("add", &add).function("place", &lua::Binding::native<&place>).function("placeAll", &lua::Binding::native<&placeAll>).function("remove", &lua::Binding::native<&remove>).function("contacts", &lua::Binding::native<&contacts>).function("rest", &lua::Binding::native<&rest>).function("resting", &resting).function("step", &step).function("clear", &clear).function("debugDraw", &lua::Binding::native<&debugDraw>).property("entityCount", &entityCount).install();
}

void OverlapWorldLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newOverlapWorld>);
    lua_setfield(L, -2, "newOverlapWorld");
}

} // namespace haylen::spatial2d
