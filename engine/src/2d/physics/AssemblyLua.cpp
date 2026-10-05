#include "2d/physics/AssemblyLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/physics/Ragdoll.hpp"
#include "haylen/2d/physics/Rope.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::ScriptedHandle<physics2d::Rope>> {
    static constexpr const char* name = "haylen.Rope";
    using Storage = physics2d::ScriptedHandle<physics2d::Rope>;
};

template <> struct Type<physics2d::ScriptedHandle<physics2d::Ragdoll>> {
    static constexpr const char* name = "haylen.Ragdoll";
    using Storage = physics2d::ScriptedHandle<physics2d::Ragdoll>;
};

} // namespace haylen::lua

namespace haylen::physics2d {

void AssemblyLua::pushWorld(lua_State* L) {
    lua_getiuservalue(L, 1, 1);
}

// Builds a rope with `newRope(world, {from, to, segments, thickness, density, friction, linearDamping, angularDamping, planks, pinStart, pinEnd, startBody, endBody, category, mask, group})`.
int AssemblyLua::createRope(lua_State* L, bool bridge) {
    World& world = lua::Userdata::check<World>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kRopeFields});
    Rope::Options options;
    lua::Table::readField(L, 2, "from", options.start);
    lua::Table::readField(L, 2, "to", options.end);
    lua::Table::readField(L, 2, "segments", options.segments);
    lua::Table::readField(L, 2, "thickness", options.thickness);
    lua::Table::readField(L, 2, "density", options.density);
    lua::Table::readField(L, 2, "friction", options.friction);
    lua::Table::readField(L, 2, "linearDamping", options.linearDamping);
    lua::Table::readField(L, 2, "angularDamping", options.angularDamping);
    lua::Table::readField(L, 2, "planks", options.planks);
    lua::Table::readField(L, 2, "pinStart", options.pinStart);
    lua::Table::readField(L, 2, "pinEnd", options.pinEnd);
    lua::Table::readField(L, 2, "limitLength", options.limitLength);
    options.filter = ShapeLua::readFilter(L, 2, {});
    if (lua_getfield(L, 2, "startBody") != LUA_TNIL) {
        options.startBody = lua::Userdata::check<ScriptedHandle<Body>>(L, -1).handle;
    }
    if (lua_getfield(L, 2, "endBody") != LUA_TNIL) {
        options.endBody = lua::Userdata::check<ScriptedHandle<Body>>(L, -1).handle;
    }
    Physics2DLua::push(L, 1, bridge ? Rope::createBridge(world, options) : Rope::create(world, options));
    return 1;
}

int AssemblyLua::newRope(lua_State* L) {
    return createRope(L, false);
}

int AssemblyLua::newBridge(lua_State* L) {
    return createRope(L, true);
}

int AssemblyLua::ropeBodies(lua_State* L) {
    const Rope& rope = lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle;
    pushWorld(L);
    Physics2DLua::pushList(L, -1, rope.getBodies());
    return 1;
}

int AssemblyLua::ropeJoints(lua_State* L) {
    const Rope& rope = lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle;
    pushWorld(L);
    Physics2DLua::pushList(L, -1, rope.getJoints());
    return 1;
}

// Returns `{x, y, rotation, length}` for each segment, which places a sprite on it.
int AssemblyLua::ropeSegments(lua_State* L) {
    const std::vector<Rope::Segment> segments = lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle.getSegments();
    lua_createtable(L, static_cast<int>(segments.size()), 0);
    for (std::size_t index = 0; index < segments.size(); ++index) {
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, segments[index].position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, segments[index].position.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, segments[index].rotation);
        lua_setfield(L, -2, "rotation");
        lua::Stack::push(L, segments[index].length);
        lua_setfield(L, -2, "length");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int AssemblyLua::ropePoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle.getPoints());
    return 1;
}

int AssemblyLua::ropeDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle.destroy();
    return 0;
}

int AssemblyLua::ropeValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle.isValid());
    return 1;
}

int AssemblyLua::ropeSegmentLength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Rope>>(L, 1).handle.getSegmentLength());
    return 1;
}

// Builds a ragdoll with `newRagdoll(world, {x, y, height, density, friction, stiffness, category, mask, group, vx, vy})`.
int AssemblyLua::newRagdoll(lua_State* L) {
    World& world = lua::Userdata::check<World>(L, 1);
    Ragdoll::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kRagdollFields});
        lua::Table::readField(L, 2, "x", options.position.x);
        lua::Table::readField(L, 2, "y", options.position.y);
        lua::Table::readField(L, 2, "height", options.height);
        lua::Table::readField(L, 2, "density", options.density);
        lua::Table::readField(L, 2, "friction", options.friction);
        lua::Table::readField(L, 2, "stiffness", options.stiffness);
        options.filter = ShapeLua::readFilter(L, 2, options.filter);
        lua::Table::readField(L, 2, "vx", options.velocity.x);
        lua::Table::readField(L, 2, "vy", options.velocity.y);
    }
    Physics2DLua::push(L, 1, Ragdoll::create(world, options));
    return 1;
}

int AssemblyLua::ragdollBody(lua_State* L) {
    const Ragdoll& ragdoll = lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle;
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const std::optional<Ragdoll::Part> part = Ragdoll::partFromName(name);
    if (!part) {
        return luaL_argerror(L, 2, "unknown ragdoll part");
    }
    pushWorld(L);
    Physics2DLua::push(L, -1, ragdoll.getBody(*part));
    return 1;
}

// Returns the bodies keyed by part name, such as `head`, `chest` and `lowerLegLeft`.
int AssemblyLua::ragdollBodies(lua_State* L) {
    const Ragdoll& ragdoll = lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle;
    pushWorld(L);
    const int world = lua_gettop(L);
    lua_createtable(L, 0, static_cast<int>(Ragdoll::kPartCount));
    for (std::size_t part = 0; part < Ragdoll::kPartCount; ++part) {
        Physics2DLua::push(L, world, ragdoll.getBodies()[part]);
        lua_setfield(L, -2, Ragdoll::partName(static_cast<Ragdoll::Part>(part)).data());
    }
    return 1;
}

int AssemblyLua::ragdollJoints(lua_State* L) {
    const Ragdoll& ragdoll = lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle;
    pushWorld(L);
    Physics2DLua::pushList(L, -1, ragdoll.getJoints());
    return 1;
}

int AssemblyLua::ragdollDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle.destroy();
    return 0;
}

int AssemblyLua::ragdollValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle.isValid());
    return 1;
}

int AssemblyLua::ragdollMass(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Ragdoll>>(L, 1).handle.getMass());
    return 1;
}

void AssemblyLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Rope>>(L).function("bodies", &lua::Binding::native<&ropeBodies>).function("joints", &lua::Binding::native<&ropeJoints>).function("segments", &lua::Binding::native<&ropeSegments>).function("points", &lua::Binding::native<&ropePoints>).function("destroy", &lua::Binding::native<&ropeDestroy>).property("valid", &ropeValid).property("segmentLength", &ropeSegmentLength).install();
    lua::ClassBuilder<ScriptedHandle<Ragdoll>>(L).function("body", &lua::Binding::native<&ragdollBody>).function("bodies", &lua::Binding::native<&ragdollBodies>).function("joints", &lua::Binding::native<&ragdollJoints>).function("destroy", &lua::Binding::native<&ragdollDestroy>).property("valid", &ragdollValid).property("mass", &lua::Binding::native<&ragdollMass>).install();
}

void AssemblyLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newRope", &lua::Binding::native<&newRope>},
        {"newBridge", &lua::Binding::native<&newBridge>},
        {"newRagdoll", &lua::Binding::native<&newRagdoll>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::physics2d
