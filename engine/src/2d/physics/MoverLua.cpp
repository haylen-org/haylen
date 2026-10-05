#include "2d/physics/MoverLua.hpp"

#include <lua.hpp>

#include <optional>

#include "2d/physics/BodyLua.hpp"
#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/physics/Mover.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::ScriptedOwner<physics2d::Mover>> {
    static constexpr const char* name = "haylen.Mover";
    using Storage = physics2d::ScriptedOwner<physics2d::Mover>;
};

} // namespace haylen::lua

namespace haylen::physics2d {

Mover& MoverLua::check(lua_State* L) {
    Mover& mover = lua::Userdata::check<ScriptedOwner<Mover>>(L, 1).object;
    if (!mover.isValid()) {
        luaL_error(L, "The mover was destroyed.");
    }
    return mover;
}

// Creates a mover with `newMover(world, {x, y, radius, height, maxSlope, stepHeight, snapDistance, pushForce, category, mask, group})`. Its user value is the world object.
int MoverLua::newMover(lua_State* L) {
    const std::shared_ptr<World>& world = lua::Userdata::checkShared<World>(L, 1);
    Mover::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kMoverFields});
        lua::Table::readField(L, 2, "x", options.position.x);
        lua::Table::readField(L, 2, "y", options.position.y);
        lua::Table::readField(L, 2, "radius", options.radius);
        lua::Table::readField(L, 2, "height", options.height);
        lua::Table::readField(L, 2, "maxSlope", options.maxSlope);
        lua::Table::readField(L, 2, "stepHeight", options.stepHeight);
        lua::Table::readField(L, 2, "snapDistance", options.snapDistance);
        lua::Table::readField(L, 2, "pushForce", options.pushForce);
        options.filter = ShapeLua::readFilter(L, 2, {});
    }
    lua::Userdata::emplace<ScriptedOwner<Mover>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int MoverLua::move(lua_State* L) {
    lua::Stack::push(L, check(L).move({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int MoverLua::clip(lua_State* L) {
    lua::Stack::push(L, check(L).clip({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

// Lets the mover pass one-way platforms with `dropThrough([seconds])`, for 0.2 seconds by default.
int MoverLua::dropThrough(lua_State* L) {
    check(L).dropThrough(static_cast<float>(luaL_optnumber(L, 2, BodyLua::kDropSeconds)));
    return 0;
}

int MoverLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Mover>>(L, 1).object.destroy();
    return 0;
}

int MoverLua::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition());
    return 1;
}

int MoverLua::setPosition(lua_State* L) {
    check(L).setPosition(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int MoverLua::getX(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().x);
    return 1;
}

int MoverLua::getY(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().y);
    return 1;
}

int MoverLua::isGrounded(lua_State* L) {
    lua::Stack::push(L, check(L).isGrounded());
    return 1;
}

int MoverLua::getGroundNormal(lua_State* L) {
    lua::Stack::push(L, check(L).getGroundNormal());
    return 1;
}

int MoverLua::getGroundBody(lua_State* L) {
    const std::optional<Body> ground = check(L).getGroundBody();
    if (!ground) {
        lua_pushnil(L);
        return 1;
    }
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, *ground);
    return 1;
}

int MoverLua::getGroundVelocity(lua_State* L) {
    lua::Stack::push(L, check(L).getGroundVelocity());
    return 1;
}

int MoverLua::isOnWall(lua_State* L) {
    lua::Stack::push(L, check(L).isOnWall());
    return 1;
}

int MoverLua::isOnCeiling(lua_State* L) {
    lua::Stack::push(L, check(L).isOnCeiling());
    return 1;
}

int MoverLua::getBody(lua_State* L) {
    const Body body = check(L).getBody();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int MoverLua::getRadius(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().radius);
    return 1;
}

int MoverLua::getHeight(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().height);
    return 1;
}

int MoverLua::getMaxSlope(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().maxSlope);
    return 1;
}

int MoverLua::setMaxSlope(lua_State* L) {
    check(L).setMaxSlope(lua::Stack::read<float>(L, 3));
    return 0;
}

int MoverLua::getStepHeight(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().stepHeight);
    return 1;
}

int MoverLua::getSnapDistance(lua_State* L) {
    lua::Stack::push(L, check(L).getOptions().snapDistance);
    return 1;
}

int MoverLua::setSnapDistance(lua_State* L) {
    check(L).setSnapDistance(lua::Stack::read<float>(L, 3));
    return 0;
}

int MoverLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Mover>>(L, 1).object.isValid());
    return 1;
}

void MoverLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<Mover>>(L).function("move", &lua::Binding::native<&move>).function("clip", &lua::Binding::native<&clip>).function("dropThrough", &lua::Binding::native<&dropThrough>).function("destroy", &lua::Binding::native<&destroy>).property("position", &lua::Binding::native<&getPosition>, &lua::Binding::native<&setPosition>).property("x", &lua::Binding::native<&getX>).property("y", &lua::Binding::native<&getY>).property("grounded", &lua::Binding::native<&isGrounded>).property("groundNormal", &lua::Binding::native<&getGroundNormal>).property("groundBody", &lua::Binding::native<&getGroundBody>).property("groundVelocity", &lua::Binding::native<&getGroundVelocity>).property("onWall", &lua::Binding::native<&isOnWall>).property("onCeiling", &lua::Binding::native<&isOnCeiling>).property("body", &lua::Binding::native<&getBody>).property("radius", &lua::Binding::native<&getRadius>).property("height", &lua::Binding::native<&getHeight>).property("maxSlope", &lua::Binding::native<&getMaxSlope>, &lua::Binding::native<&setMaxSlope>).property("stepHeight", &lua::Binding::native<&getStepHeight>).property("snapDistance", &lua::Binding::native<&getSnapDistance>, &lua::Binding::native<&setSnapDistance>).property("valid", &lua::Binding::native<&isValid>).install();
}

void MoverLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newMover>);
    lua_setfield(L, -2, "newMover");
}

} // namespace haylen::physics2d
