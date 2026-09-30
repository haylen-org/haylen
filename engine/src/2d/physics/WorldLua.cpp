#include "2d/physics/WorldLua.hpp"

#include <algorithm>
#include <cstdint>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "2d/physics/WorldRaycastLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

World& WorldLua::check(lua_State* L) {
    return lua::Userdata::check<World>(L, 1);
}

// Reads `{type, x, y, rotation, vx, vy, angularVelocity, linearDamping, angularDamping, gravityScale, fixedRotation, bullet, sleep}`.
Body::Options WorldLua::readBodyOptions(lua_State* L, int index) {
    Body::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kBodyFields});
    lua::Table::readField(L, index, "type", options.type);
    lua::Table::readField(L, index, "x", options.position.x);
    lua::Table::readField(L, index, "y", options.position.y);
    lua::Table::readField(L, index, "rotation", options.rotation);
    lua::Table::readField(L, index, "vx", options.velocity.x);
    lua::Table::readField(L, index, "vy", options.velocity.y);
    lua::Table::readField(L, index, "angularVelocity", options.angularVelocity);
    lua::Table::readField(L, index, "linearDamping", options.linearDamping);
    lua::Table::readField(L, index, "angularDamping", options.angularDamping);
    lua::Table::readField(L, index, "gravityScale", options.gravityScale);
    lua::Table::readField(L, index, "fixedRotation", options.fixedRotation);
    lua::Table::readField(L, index, "bullet", options.bullet);
    lua::Table::readField(L, index, "sleepEnabled", options.sleepEnabled);
    return options;
}

// Reads `{ax, ay, bx, by, lower, upper, motorSpeed, maxMotorForce, maxMotorTorque, hertz, ...}`.
Joint::Options WorldLua::readJointOptions(lua_State* L, int index) {
    Joint::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kJointFields});
    lua::Table::readField(L, index, "ax", options.anchorA.x);
    lua::Table::readField(L, index, "ay", options.anchorA.y);
    lua::Table::readField(L, index, "bx", options.anchorB.x);
    lua::Table::readField(L, index, "by", options.anchorB.y);
    lua::Table::readField(L, index, "collideConnected", options.collideConnected);
    lua::Table::readField(L, index, "enableLimit", options.enableLimit);
    lua::Table::readField(L, index, "lower", options.lower);
    lua::Table::readField(L, index, "upper", options.upper);
    lua::Table::readField(L, index, "enableMotor", options.enableMotor);
    lua::Table::readField(L, index, "motorSpeed", options.motorSpeed);
    lua::Table::readField(L, index, "maxMotorForce", options.maxMotorForce);
    lua::Table::readField(L, index, "maxMotorTorque", options.maxMotorTorque);
    lua::Table::readField(L, index, "enableSpring", options.enableSpring);
    lua::Table::readField(L, index, "hertz", options.hertz);
    lua::Table::readField(L, index, "dampingRatio", options.dampingRatio);
    lua::Table::readField(L, index, "axisX", options.axis.x);
    lua::Table::readField(L, index, "axisY", options.axis.y);
    lua::Table::readField(L, index, "length", options.length);
    return options;
}

CollisionFilter WorldLua::readQueryFilter(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return {};
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kFilterFields});
    return ShapeLua::readFilter(L, index, {});
}

void WorldLua::pushContact(lua_State* L, int worldIndex, const ContactEvent& event) {
    lua_createtable(L, 0, 7);
    Physics2DLua::push(L, worldIndex, event.first);
    lua_setfield(L, -2, "shapeA");
    Physics2DLua::push(L, worldIndex, event.second);
    lua_setfield(L, -2, "shapeB");
    lua::Stack::push(L, event.point.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, event.point.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, event.normal.x);
    lua_setfield(L, -2, "normalX");
    lua::Stack::push(L, event.normal.y);
    lua_setfield(L, -2, "normalY");
    lua::Stack::push(L, event.speed);
    lua_setfield(L, -2, "speed");
}

void WorldLua::pushBodyOf(lua_State* L, int worldIndex, const Shape& shape) {
    if (!shape.isValid()) {
        lua_pushnil(L);
        return;
    }
    Physics2DLua::push(L, worldIndex, shape.getBody());
}

bool WorldLua::pushCallback(lua_State* L, int worldIndex, const char* name) {
    lua_getiuservalue(L, worldIndex, 1);
    if (lua_getfield(L, -1, name) != LUA_TFUNCTION) {
        lua_pop(L, 2);
        return false;
    }
    lua_remove(L, -2);
    return true;
}

void WorldLua::dispatchContacts(lua_State* L, int worldIndex, const char* name, const std::vector<ContactEvent>& events) {
    for (const ContactEvent& event : events) {
        if (!pushCallback(L, worldIndex, name)) {
            return;
        }
        pushBodyOf(L, worldIndex, event.first);
        pushBodyOf(L, worldIndex, event.second);
        pushContact(L, worldIndex, event);
        lua_call(L, 3, 0);
    }
}

void WorldLua::dispatchSensors(lua_State* L, int worldIndex, const char* name, const std::vector<SensorEvent>& events) {
    for (const SensorEvent& event : events) {
        if (!pushCallback(L, worldIndex, name)) {
            return;
        }
        pushBodyOf(L, worldIndex, event.sensor);
        pushBodyOf(L, worldIndex, event.visitor);
        lua_createtable(L, 0, 2);
        Physics2DLua::push(L, worldIndex, event.sensor);
        lua_setfield(L, -2, "sensorShape");
        Physics2DLua::push(L, worldIndex, event.visitor);
        lua_setfield(L, -2, "visitorShape");
        lua_call(L, 3, 0);
    }
}

// Creates a body with `createBody({type, x, y, rotation, vx, vy, angularVelocity, linearDamping, angularDamping, gravityScale, fixedRotation, bullet, sleep})`.
int WorldLua::createBody(lua_State* L) {
    World& world = check(L);
    Physics2DLua::push(L, 1, world.createBody(readBodyOptions(L, 2)));
    return 1;
}

// Joins two bodies with `createJoint(type, a, b, {ax, ay, bx, by, lower, upper, motorSpeed, maxMotorForce, maxMotorTorque, hertz, ...})`.
int WorldLua::createJoint(lua_State* L) {
    World& world = check(L);
    const auto type = lua::Stack::read<Joint::Type>(L, 2);
    const Body first = lua::Userdata::check<ScriptedHandle<Body>>(L, 3).handle;
    const Body second = lua::Userdata::check<ScriptedHandle<Body>>(L, 4).handle;
    Physics2DLua::push(L, 1, world.createJoint(type, first, second, readJointOptions(L, 5)));
    return 1;
}

std::vector<Body> WorldLua::readBodies(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<Body> bodies;
    bodies.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        bodies.push_back(lua::Userdata::check<ScriptedHandle<Body>>(L, -1).handle);
        lua_pop(L, 1);
    }
    return bodies;
}

std::span<float> WorldLua::readTransformValues(lua_State* L, int index) {
    const std::span<float> values = lua::Userdata::check<core::FloatBuffer>(L, index).getValues();
    const lua_Integer first = luaL_optinteger(L, index + 1, 1);
    luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first) <= values.size() + 1, index + 1, "the first position is outside the buffer");
    return values.subspan(static_cast<std::size_t>(first - 1));
}

// Copies `x`, `y` and `rotation` of every body into a float buffer with `readTransforms(bodies, buffer[, first])`.
int WorldLua::readTransforms(lua_State* L) {
    const std::vector<Body> bodies = readBodies(L, 2);
    check(L).readTransforms(bodies, readTransformValues(L, 3));
    return 0;
}

// Moves every body to the `x`, `y` and `rotation` a float buffer holds with `writeTransforms(bodies, buffer[, first])`.
int WorldLua::writeTransforms(lua_State* L) {
    const std::vector<Body> bodies = readBodies(L, 2);
    check(L).writeTransforms(bodies, readTransformValues(L, 3));
    return 0;
}

bool WorldLua::hasCallbacks(lua_State* L, int worldIndex) {
    lua_getiuservalue(L, worldIndex, 1);
    // clang-format off
    const bool found = std::ranges::any_of(kCallbacks, [L](std::string_view name) {
        const bool set = lua_getfield(L, -1, name.data()) == LUA_TFUNCTION;
        lua_pop(L, 1);
        return set;
    });
    // clang-format on
    lua_pop(L, 1);
    return found;
}

// Bodies also die outside `body:destroy()`, in fractures, assemblies, fluids, terrains and the garbage collection of their owners, so every step drops the data of the bodies that are gone.
void WorldLua::releaseDestroyedData(lua_State* L, int worldIndex) {
    World& world = lua::Userdata::check<World>(L, worldIndex);
    lua_getiuservalue(L, worldIndex, 1);
    lua_getfield(L, -1, "data");
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        lua_pop(L, 1);
        if (!Body(&world, static_cast<std::uint64_t>(lua_tointeger(L, -1))).isValid()) {
            lua_pushvalue(L, -1);
            lua_pushnil(L);
            lua_rawset(L, -4);
        }
    }
    lua_pop(L, 2);
}

// Releases the data of destroyed bodies, advances the world and then calls the event callbacks set on it.
int WorldLua::step(lua_State* L) {
    World& world = check(L);
    releaseDestroyedData(L, 1);
    world.step(lua::Stack::read<float>(L, 2));
    if (!hasCallbacks(L, 1)) {
        return 0;
    }

    // A callback may step the world again, which replaces its event lists, so every list is copied before the first callback runs.
    const std::vector<ContactEvent> begins = world.getContactBegins();
    const std::vector<ContactEvent> ends = world.getContactEnds();
    const std::vector<ContactEvent> hits = world.getContactHits();
    const std::vector<SensorEvent> sensorBegins = world.getSensorBegins();
    const std::vector<SensorEvent> sensorEnds = world.getSensorEnds();

    dispatchContacts(L, 1, "onContactBegin", begins);
    dispatchContacts(L, 1, "onContactEnd", ends);
    dispatchContacts(L, 1, "onHit", hits);
    dispatchSensors(L, 1, "onSensorBegin", sensorBegins);
    dispatchSensors(L, 1, "onSensorEnd", sensorEnds);
    return 0;
}

int WorldLua::queryRect(lua_State* L) {
    Physics2DLua::pushList(L, 1, check(L).queryRect(lua::Stack::read<math::Rect>(L, 2), readQueryFilter(L, 3)));
    return 1;
}

int WorldLua::queryCircle(lua_State* L) {
    Physics2DLua::pushList(L, 1, check(L).queryCircle({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4), readQueryFilter(L, 5)));
    return 1;
}

int WorldLua::queryPoint(lua_State* L) {
    Physics2DLua::pushList(L, 1, check(L).queryPoint({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readQueryFilter(L, 4)));
    return 1;
}

int WorldLua::debugDraw(lua_State* L) {
    check(L).debugDraw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int WorldLua::getGravity(lua_State* L) {
    lua::Stack::push(L, check(L).getGravity());
    return 1;
}

int WorldLua::setGravity(lua_State* L) {
    check(L).setGravity(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int WorldLua::getBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getBodyCount());
    return 1;
}

int WorldLua::getPixelsPerMeter(lua_State* L) {
    lua::Stack::push(L, check(L).getPixelsPerMeter());
    return 1;
}

template <std::size_t Index> int WorldLua::getCallback(lua_State* L) {
    (void)check(L);
    lua_getiuservalue(L, 1, 1);
    lua_getfield(L, -1, kCallbacks[Index].data());
    return 1;
}

template <std::size_t Index> int WorldLua::setCallback(lua_State* L) {
    (void)check(L);
    if (!lua_isnil(L, 3)) {
        luaL_checktype(L, 3, LUA_TFUNCTION);
    }
    lua_getiuservalue(L, 1, 1);
    lua_pushvalue(L, 3);
    lua_setfield(L, -2, kCallbacks[Index].data());
    return 0;
}

void WorldLua::install(lua_State* L) {
    lua::ClassBuilder<World>(L).function("createBody", &lua::Binding::native<&createBody>).function("createJoint", &lua::Binding::native<&createJoint>).function("step", &lua::Binding::native<&step>).function("readTransforms", &lua::Binding::native<&readTransforms>).function("writeTransforms", &lua::Binding::native<&writeTransforms>).function("raycast", &lua::Binding::native<&WorldRaycastLua::raycast>).function("raycastAll", &lua::Binding::native<&WorldRaycastLua::raycastAll>).function("castCircle", &lua::Binding::native<&WorldRaycastLua::castCircle>).function("castBox", &lua::Binding::native<&WorldRaycastLua::castBox>).function("castCapsule", &lua::Binding::native<&WorldRaycastLua::castCapsule>).function("castPolygon", &lua::Binding::native<&WorldRaycastLua::castPolygon>).function("bounceRay", &lua::Binding::native<&WorldRaycastLua::bounceRay>).function("rayFan", &lua::Binding::native<&WorldRaycastLua::rayFan>).function("lineOfSight", &lua::Binding::native<&WorldRaycastLua::lineOfSight>).function("raycastBatch", &lua::Binding::native<&WorldRaycastLua::raycastBatch>).function("pick", &lua::Binding::native<&WorldRaycastLua::pick>).function("debugDrawRays", &lua::Binding::native<&WorldRaycastLua::debugDrawRays>).property("debugRays", &WorldRaycastLua::isDebuggingRays, &lua::Binding::native<&WorldRaycastLua::setDebuggingRays>).function("queryRect", &lua::Binding::native<&queryRect>).function("queryCircle", &lua::Binding::native<&queryCircle>).function("queryPoint", &lua::Binding::native<&queryPoint>).function("debugDraw", &lua::Binding::native<&debugDraw>).property("gravity", &getGravity, &lua::Binding::native<&setGravity>).property("bodyCount", &getBodyCount).property("pixelsPerMeter", &getPixelsPerMeter).property("onContactBegin", &getCallback<0>, &setCallback<0>).property("onContactEnd", &getCallback<1>, &setCallback<1>).property("onHit", &getCallback<2>, &setCallback<2>).property("onSensorBegin", &getCallback<3>, &setCallback<3>).property("onSensorEnd", &getCallback<4>, &setCallback<4>).install();
}

} // namespace haylen::physics2d
