#include "2d/physics/WorldLua.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "2d/physics/WorldRaycastLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/2d/physics/PathPredictor.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
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
    lua::Table::readField(L, index, "fastRotation", options.fastRotation);
    lua::Table::readField(L, index, "sleepEnabled", options.sleepEnabled);
    lua::Table::readField(L, index, "sleepThreshold", options.sleepThreshold);
    return options;
}

// Reads `{ax, ay, bx, by, lower, upper, motorSpeed, maxMotorForce, maxMotorTorque, hertz, breakForce, ...}`.
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
    lua::Table::readField(L, index, "targetAngle", options.targetAngle);
    lua::Table::readField(L, index, "targetTranslation", options.targetTranslation);
    lua::Table::readField(L, index, "axisX", options.axis.x);
    lua::Table::readField(L, index, "axisY", options.axis.y);
    lua::Table::readField(L, index, "length", options.length);
    lua::Table::readField(L, index, "breakForce", options.breakForce);
    lua::Table::readField(L, index, "breakTorque", options.breakTorque);
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

bool WorldLua::hasCallback(lua_State* L, int worldIndex, const char* name) {
    if (!pushCallback(L, worldIndex, name)) {
        return false;
    }
    lua_pop(L, 1);
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

// Calls `onJointBreak(joint, info)` with the broken joint, which reports `valid == false`, and `{bodyA, bodyB, forceX, forceY, torque}`.
void WorldLua::dispatchJointBreaks(lua_State* L, int worldIndex, const std::vector<World::JointBreak>& breaks) {
    for (const World::JointBreak& broken : breaks) {
        if (!pushCallback(L, worldIndex, "onJointBreak")) {
            return;
        }
        Physics2DLua::push(L, worldIndex, broken.joint);
        lua_createtable(L, 0, 5);
        if (broken.first.isValid()) {
            Physics2DLua::push(L, worldIndex, broken.first);
            lua_setfield(L, -2, "bodyA");
        }
        if (broken.second.isValid()) {
            Physics2DLua::push(L, worldIndex, broken.second);
            lua_setfield(L, -2, "bodyB");
        }
        lua::Stack::push(L, broken.force.x);
        lua_setfield(L, -2, "forceX");
        lua::Stack::push(L, broken.force.y);
        lua_setfield(L, -2, "forceY");
        lua::Stack::push(L, broken.torque);
        lua_setfield(L, -2, "torque");
        lua_call(L, 2, 0);
    }
}

// Creates a body with `createBody({type, x, y, rotation, vx, vy, angularVelocity, linearDamping, angularDamping, gravityScale, fixedRotation, bullet, fastRotation, sleepEnabled, sleepThreshold})`.
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

// Copies `x`, `y` and `rotation` of every body into a float buffer with `readTransforms(bodies, buffer[, first[, interpolated]])`, blended between the last two steps by the interpolation of the frame clock when `interpolated` is `true`.
int WorldLua::readTransforms(lua_State* L) {
    const std::vector<Body> bodies = readBodies(L, 2);
    std::optional<float> blend;
    if (lua_toboolean(L, 5) != 0) {
        blend = static_cast<float>(lua::Runtime::getEngine(L).getClock().getInterpolation());
    }
    check(L).readTransforms(bodies, readTransformValues(L, 3), blend);
    return 0;
}

// Moves every body to the `x`, `y` and `rotation` a float buffer holds with `writeTransforms(bodies, buffer[, first])`.
int WorldLua::writeTransforms(lua_State* L) {
    const std::vector<Body> bodies = readBodies(L, 2);
    check(L).writeTransforms(bodies, readTransformValues(L, 3));
    return 0;
}

// Bodies also die outside `body:destroy()`, in fractures, assemblies, terrains and the garbage collection of their owners, so every step drops the data of the bodies the world saw go.
void WorldLua::releaseDestroyedData(lua_State* L, int worldIndex) {
    std::vector<std::uint64_t> destroyed;
    lua::Userdata::check<World>(L, worldIndex).takeDestroyedBodies(destroyed);
    if (destroyed.empty()) {
        return;
    }
    lua_getiuservalue(L, worldIndex, 1);
    lua_getfield(L, -1, "data");
    for (const std::uint64_t id : destroyed) {
        lua_pushnil(L);
        lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    }
    lua_pop(L, 2);
}

// Releases the data of destroyed bodies, advances the world and then calls the event callbacks set on it, reading only the events some callback wants.
int WorldLua::step(lua_State* L) {
    World& world = check(L);
    releaseDestroyedData(L, 1);
    world.step(lua::Stack::read<float>(L, 2));

    // A callback may step the world again, which replaces its event lists, so every list a callback wants is copied before the first callback runs.
    const bool begins = hasCallback(L, 1, "onContactBegin");
    const bool ends = hasCallback(L, 1, "onContactEnd");
    const bool hits = hasCallback(L, 1, "onHit");
    const bool sensorBegins = hasCallback(L, 1, "onSensorBegin");
    const bool sensorEnds = hasCallback(L, 1, "onSensorEnd");
    const bool breaks = hasCallback(L, 1, "onJointBreak") && !world.getJointBreaks().empty();
    if (!begins && !ends && !hits && !sensorBegins && !sensorEnds && !breaks) {
        return 0;
    }
    const std::vector<ContactEvent> beginEvents = begins ? world.getContactBegins() : std::vector<ContactEvent>{};
    const std::vector<ContactEvent> endEvents = ends ? world.getContactEnds() : std::vector<ContactEvent>{};
    const std::vector<ContactEvent> hitEvents = hits ? world.getContactHits() : std::vector<ContactEvent>{};
    const std::vector<SensorEvent> sensorBeginEvents = sensorBegins ? world.getSensorBegins() : std::vector<SensorEvent>{};
    const std::vector<SensorEvent> sensorEndEvents = sensorEnds ? world.getSensorEnds() : std::vector<SensorEvent>{};
    const std::vector<World::JointBreak> breakEvents = breaks ? world.getJointBreaks() : std::vector<World::JointBreak>{};

    dispatchContacts(L, 1, "onContactBegin", beginEvents);
    dispatchContacts(L, 1, "onContactEnd", endEvents);
    dispatchContacts(L, 1, "onHit", hitEvents);
    dispatchSensors(L, 1, "onSensorBegin", sensorBeginEvents);
    dispatchSensors(L, 1, "onSensorEnd", sensorEndEvents);
    dispatchJointBreaks(L, 1, breakEvents);
    return 0;
}

int WorldLua::wakeAll(lua_State* L) {
    check(L).wakeAll();
    return 0;
}

// Returns `{bodies, awakeBodies, shapes, contacts, joints, islands, stepMilliseconds, collideMilliseconds, solveMilliseconds, continuousMilliseconds, sleepMilliseconds, hookMilliseconds}`.
int WorldLua::stats(lua_State* L) {
    const World::Stats stats = check(L).getStats();
    lua_createtable(L, 0, 12);
    // clang-format off
    const auto set = [L](const char* name, auto value) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    };
    // clang-format on
    set("bodies", stats.bodies);
    set("awakeBodies", stats.awakeBodies);
    set("shapes", stats.shapes);
    set("contacts", stats.contacts);
    set("joints", stats.joints);
    set("islands", stats.islands);
    set("stepMilliseconds", stats.stepMilliseconds);
    set("collideMilliseconds", stats.collideMilliseconds);
    set("solveMilliseconds", stats.solveMilliseconds);
    set("continuousMilliseconds", stats.continuousMilliseconds);
    set("sleepMilliseconds", stats.sleepMilliseconds);
    set("hookMilliseconds", stats.hookMilliseconds);
    return 1;
}

// Returns the hash of the transforms and velocities of a list of bodies with `stateHash(bodies)`, as an integer.
int WorldLua::stateHash(lua_State* L) {
    const std::vector<Body> bodies = readBodies(L, 2);
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).computeStateHash(bodies)));
    return 1;
}

// Predicts a throw with `predictPath(x, y, vx, vy, {steps, step, radius, gravityScale, linearDamping, category, mask, group})` and returns the list of points and the hit or `nil`.
int WorldLua::predictPath(lua_State* L) {
    const World& world = check(L);
    PathPredictor::Options options{.step = static_cast<float>(lua::Runtime::getEngine(L).getClock().getFixedStep())};
    if (!lua_isnoneornil(L, 6)) {
        luaL_checktype(L, 6, LUA_TTABLE);
        lua::Table::checkFields(L, 6, {kPredictFields, kFilterFields});
        lua::Table::readField(L, 6, "steps", options.steps);
        lua::Table::readField(L, 6, "step", options.step);
        lua::Table::readField(L, 6, "radius", options.radius);
        lua::Table::readField(L, 6, "gravityScale", options.gravityScale);
        lua::Table::readField(L, 6, "linearDamping", options.linearDamping);
        options.filter = ShapeLua::readFilter(L, 6, {});
    }
    const math::Vec2 position{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const math::Vec2 velocity{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const PathPredictor::Path path = PathPredictor::predict(world, position, velocity, options);
    lua::Stack::push(L, path.points);
    if (path.hit) {
        WorldRaycastLua::pushHit(L, 1, *path.hit);
    } else {
        lua_pushnil(L);
    }
    return 2;
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

int WorldLua::getAwakeBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getAwakeBodyCount());
    return 1;
}

int WorldLua::getPixelsPerMeter(lua_State* L) {
    lua::Stack::push(L, check(L).getPixelsPerMeter());
    return 1;
}

int WorldLua::getThreads(lua_State* L) {
    lua::Stack::push(L, check(L).getThreads());
    return 1;
}

int WorldLua::isInterpolating(lua_State* L) {
    lua::Stack::push(L, check(L).isInterpolating());
    return 1;
}

int WorldLua::getSubSteps(lua_State* L) {
    lua::Stack::push(L, check(L).getSubSteps());
    return 1;
}

int WorldLua::setSubSteps(lua_State* L) {
    check(L).setSubSteps(lua::Stack::read<int>(L, 3));
    return 0;
}

int WorldLua::isContinuous(lua_State* L) {
    lua::Stack::push(L, check(L).isContinuousEnabled());
    return 1;
}

int WorldLua::setContinuous(lua_State* L) {
    check(L).setContinuousEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int WorldLua::isSleepEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isSleepEnabled());
    return 1;
}

int WorldLua::setSleepEnabled(lua_State* L) {
    check(L).setSleepEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int WorldLua::getMaxSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxSpeed());
    return 1;
}

int WorldLua::setMaxSpeed(lua_State* L) {
    check(L).setMaxSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldLua::getContactHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getContactHertz());
    return 1;
}

int WorldLua::setContactHertz(lua_State* L) {
    check(L).setContactHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldLua::getContactDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getContactDampingRatio());
    return 1;
}

int WorldLua::setContactDampingRatio(lua_State* L) {
    check(L).setContactDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldLua::getContactPushSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getContactPushSpeed());
    return 1;
}

int WorldLua::setContactPushSpeed(lua_State* L) {
    check(L).setContactPushSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldLua::getRestitutionThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getRestitutionThreshold());
    return 1;
}

int WorldLua::setRestitutionThreshold(lua_State* L) {
    check(L).setRestitutionThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldLua::getHitThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getHitThreshold());
    return 1;
}

int WorldLua::setHitThreshold(lua_State* L) {
    check(L).setHitThreshold(lua::Stack::read<float>(L, 3));
    return 0;
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
    lua::ClassBuilder<World>(L).function("createBody", &lua::Binding::native<&createBody>).function("createJoint", &lua::Binding::native<&createJoint>).function("step", &lua::Binding::native<&step>).function("wakeAll", &lua::Binding::native<&wakeAll>).function("stats", &lua::Binding::native<&stats>).function("stateHash", &lua::Binding::native<&stateHash>).function("predictPath", &lua::Binding::native<&predictPath>).function("readTransforms", &lua::Binding::native<&readTransforms>).function("writeTransforms", &lua::Binding::native<&writeTransforms>).function("raycast", &lua::Binding::native<&WorldRaycastLua::raycast>).function("raycastAll", &lua::Binding::native<&WorldRaycastLua::raycastAll>).function("castCircle", &lua::Binding::native<&WorldRaycastLua::castCircle>).function("castBox", &lua::Binding::native<&WorldRaycastLua::castBox>).function("castCapsule", &lua::Binding::native<&WorldRaycastLua::castCapsule>).function("castPolygon", &lua::Binding::native<&WorldRaycastLua::castPolygon>).function("bounceRay", &lua::Binding::native<&WorldRaycastLua::bounceRay>).function("rayFan", &lua::Binding::native<&WorldRaycastLua::rayFan>).function("lineOfSight", &lua::Binding::native<&WorldRaycastLua::lineOfSight>).function("raycastBatch", &lua::Binding::native<&WorldRaycastLua::raycastBatch>).function("pick", &lua::Binding::native<&WorldRaycastLua::pick>).function("debugDrawRays", &lua::Binding::native<&WorldRaycastLua::debugDrawRays>).property("debugRays", &WorldRaycastLua::isDebuggingRays, &lua::Binding::native<&WorldRaycastLua::setDebuggingRays>).function("queryRect", &lua::Binding::native<&queryRect>).function("queryCircle", &lua::Binding::native<&queryCircle>).function("queryPoint", &lua::Binding::native<&queryPoint>).function("debugDraw", &lua::Binding::native<&debugDraw>).property("gravity", &getGravity, &lua::Binding::native<&setGravity>).property("bodyCount", &getBodyCount).property("awakeBodyCount", &getAwakeBodyCount).property("pixelsPerMeter", &getPixelsPerMeter).property("threads", &getThreads).property("interpolate", &isInterpolating).property("subSteps", &getSubSteps, &lua::Binding::native<&setSubSteps>).property("continuous", &isContinuous, &lua::Binding::native<&setContinuous>).property("sleepEnabled", &isSleepEnabled, &lua::Binding::native<&setSleepEnabled>).property("maxSpeed", &getMaxSpeed, &lua::Binding::native<&setMaxSpeed>).property("contactHertz", &getContactHertz, &lua::Binding::native<&setContactHertz>).property("contactDampingRatio", &getContactDampingRatio, &lua::Binding::native<&setContactDampingRatio>).property("contactPushSpeed", &getContactPushSpeed, &lua::Binding::native<&setContactPushSpeed>).property("restitutionThreshold", &getRestitutionThreshold, &lua::Binding::native<&setRestitutionThreshold>).property("hitThreshold", &getHitThreshold, &lua::Binding::native<&setHitThreshold>).property("onContactBegin", &getCallback<0>, &setCallback<0>).property("onContactEnd", &getCallback<1>, &setCallback<1>).property("onHit", &getCallback<2>, &setCallback<2>).property("onSensorBegin", &getCallback<3>, &setCallback<3>).property("onSensorEnd", &getCallback<4>, &setCallback<4>).property("onJointBreak", &getCallback<5>, &setCallback<5>).install();
}

} // namespace haylen::physics2d
