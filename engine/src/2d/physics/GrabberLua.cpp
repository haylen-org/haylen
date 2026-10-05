#include "2d/physics/GrabberLua.hpp"

#include <lua.hpp>

#include <optional>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/physics/Grabber.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::ScriptedOwner<physics2d::Grabber>> {
    static constexpr const char* name = "haylen.Grabber";
    using Storage = physics2d::ScriptedOwner<physics2d::Grabber>;
};

} // namespace haylen::lua

namespace haylen::physics2d {

// Creates a grabber with `newGrabber(world, {pickRadius, strength, hertz, dampingRatio, category, mask})`. Its user value is the world object.
int GrabberLua::newGrabber(lua_State* L) {
    const std::shared_ptr<World>& world = lua::Userdata::checkShared<World>(L, 1);
    Grabber::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kGrabberFields});
        lua::Table::readField(L, 2, "pickRadius", options.pickRadius);
        lua::Table::readField(L, 2, "strength", options.strength);
        lua::Table::readField(L, 2, "hertz", options.hertz);
        lua::Table::readField(L, 2, "dampingRatio", options.dampingRatio);
        options.filter = ShapeLua::readFilter(L, 2, {});
    }
    lua::Userdata::emplace<ScriptedOwner<Grabber>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

// Takes the body nearest to a world point with `grab(x, y)` and returns it, or `nil` when nothing is within the pick radius.
int GrabberLua::grab(lua_State* L) {
    const std::optional<Body> body = lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.grab({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    if (!body) {
        lua_pushnil(L);
        return 1;
    }
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, *body);
    return 1;
}

int GrabberLua::moveTo(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.moveTo({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int GrabberLua::release(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.release();
    return 0;
}

int GrabberLua::isHolding(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.isHolding());
    return 1;
}

int GrabberLua::getBody(lua_State* L) {
    const std::optional<Body> body = lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getBody();
    if (!body) {
        lua_pushnil(L);
        return 1;
    }
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, *body);
    return 1;
}

int GrabberLua::getTarget(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getTarget());
    return 1;
}

int GrabberLua::getHandle(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getHandle());
    return 1;
}

int GrabberLua::getForce(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getForce());
    return 1;
}

int GrabberLua::getPickRadius(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getOptions().pickRadius);
    return 1;
}

int GrabberLua::setPickRadius(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.setPickRadius(lua::Stack::read<float>(L, 3));
    return 0;
}

int GrabberLua::getStrength(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getOptions().strength);
    return 1;
}

int GrabberLua::setStrength(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.setStrength(lua::Stack::read<float>(L, 3));
    return 0;
}

int GrabberLua::getHertz(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getOptions().hertz);
    return 1;
}

int GrabberLua::setHertz(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.setHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int GrabberLua::getDampingRatio(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.getOptions().dampingRatio);
    return 1;
}

int GrabberLua::setDampingRatio(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Grabber>>(L, 1).object.setDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

void GrabberLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<Grabber>>(L).function("grab", &lua::Binding::native<&grab>).function("moveTo", &lua::Binding::native<&moveTo>).function("release", &lua::Binding::native<&release>).property("holding", &lua::Binding::native<&isHolding>).property("body", &lua::Binding::native<&getBody>).property("target", &lua::Binding::native<&getTarget>).property("handle", &lua::Binding::native<&getHandle>).property("force", &lua::Binding::native<&getForce>).property("pickRadius", &lua::Binding::native<&getPickRadius>, &lua::Binding::native<&setPickRadius>).property("strength", &lua::Binding::native<&getStrength>, &lua::Binding::native<&setStrength>).property("hertz", &lua::Binding::native<&getHertz>, &lua::Binding::native<&setHertz>).property("dampingRatio", &lua::Binding::native<&getDampingRatio>, &lua::Binding::native<&setDampingRatio>).install();
}

void GrabberLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newGrabber>);
    lua_setfield(L, -2, "newGrabber");
}

} // namespace haylen::physics2d
