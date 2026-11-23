#include "2d/physics/WorldBinding.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "2d/physics/WorldRaycastBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/2d/physics/PathPredictor.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::physics2d {

haylen::physics2d::World& WorldBinding::check(lua_State* L) {
    haylen::physics2d::World& world = lua::Userdata::check<haylen::physics2d::World>(L, 1);
    if (world.isDestroyed()) {
        luaL_error(L, "The physics world was destroyed.");
    }

    return world;
}

void WorldBinding::resetState(lua_State* L, int index) {
    const int world = lua_absindex(L, index);
    lua_createtable(L, 0, static_cast<int>(kCallbacks.size()));
    lua_newtable(L);
    lua_setfield(L, -2, "data");
    lua_setiuservalue(L, world, 1);
}

haylen::physics2d::Body::Options WorldBinding::readBodyOptions(lua_State* L, int index) {
    haylen::physics2d::Body::Options options;
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
    lua::Table::readField(L, index, "lockX", options.locks.x);
    lua::Table::readField(L, index, "lockY", options.locks.y);
    lua::Table::readField(L, index, "lockRotation", options.locks.rotation);
    lua::Table::readField(L, index, "bullet", options.bullet);
    lua::Table::readField(L, index, "continuousThreshold", options.continuousThreshold);
    lua::Table::readField(L, index, "fastRotation", options.fastRotation);
    lua::Table::readField(L, index, "sleepEnabled", options.sleepEnabled);
    lua::Table::readField(L, index, "sleepThreshold", options.sleepThreshold);
    return options;
}

// Reads `{ax, ay, bx, by, lower, upper, motorSpeed, maxMotorForce, maxMotorTorque, hertz, breakForce, ...}`.
haylen::physics2d::Joint::Options WorldBinding::readJointOptions(lua_State* L, int index) {
    haylen::physics2d::Joint::Options options;
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

haylen::physics2d::CollisionFilter WorldBinding::readQueryFilter(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return {};
    }

    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kFilterFields});
    return ShapeBinding::readFilter(L, index, {});
}

void WorldBinding::pushContact(lua_State* L, int worldIndex, const haylen::physics2d::ContactEvent& event) {
    lua_createtable(L, 0, 12);
    Physics2DBinding::push(L, worldIndex, event.first);
    lua_setfield(L, -2, "shapeA");
    Physics2DBinding::push(L, worldIndex, event.second);
    lua_setfield(L, -2, "shapeB");
    Physics2DBinding::setManifold(L, event.manifold);
}

void WorldBinding::pushBodyOf(lua_State* L, int worldIndex, const haylen::physics2d::Shape& shape) {
    if (!shape.isValid()) {
        lua_pushnil(L);
        return;
    }

    Physics2DBinding::push(L, worldIndex, shape.getBody());
}

bool WorldBinding::pushCallback(lua_State* L, int worldIndex, const char* name) {
    lua_getiuservalue(L, worldIndex, 1);
    if (lua_getfield(L, -1, name) != LUA_TFUNCTION) {
        lua_pop(L, 2);
        return false;
    }

    lua_remove(L, -2);
    return true;
}

bool WorldBinding::hasCallback(lua_State* L, int worldIndex, const char* name) {
    if (!pushCallback(L, worldIndex, name)) {
        return false;
    }

    lua_pop(L, 1);
    return true;
}

void WorldBinding::dispatchContacts(lua_State* L, int worldIndex, const char* name, const std::vector<haylen::physics2d::ContactEvent>& events) {
    for (const haylen::physics2d::ContactEvent& event : events) {
        if (!pushCallback(L, worldIndex, name)) {
            return;
        }

        pushBodyOf(L, worldIndex, event.first);
        pushBodyOf(L, worldIndex, event.second);
        pushContact(L, worldIndex, event);
        lua_call(L, 3, 0);
    }
}

void WorldBinding::dispatchSensors(lua_State* L, int worldIndex, const char* name, const std::vector<haylen::physics2d::SensorEvent>& events) {
    for (const haylen::physics2d::SensorEvent& event : events) {
        if (!pushCallback(L, worldIndex, name)) {
            return;
        }

        pushBodyOf(L, worldIndex, event.sensor);
        pushBodyOf(L, worldIndex, event.visitor);
        lua_createtable(L, 0, 2);
        Physics2DBinding::push(L, worldIndex, event.sensor);
        lua_setfield(L, -2, "sensorShape");
        Physics2DBinding::push(L, worldIndex, event.visitor);
        lua_setfield(L, -2, "visitorShape");
        lua_call(L, 3, 0);
    }
}

// Calls `onJointBreak(joint, info)` with the broken joint, which reports `valid == false`, and `{bodyA, bodyB, forceX, forceY, torque}`.
void WorldBinding::dispatchJointBreaks(lua_State* L, int worldIndex, const std::vector<haylen::physics2d::World::JointBreak>& breaks) {
    for (const haylen::physics2d::World::JointBreak& broken : breaks) {
        if (!pushCallback(L, worldIndex, "onJointBreak")) {
            return;
        }

        Physics2DBinding::push(L, worldIndex, broken.joint);
        lua_createtable(L, 0, 5);
        if (broken.first.isValid()) {
            Physics2DBinding::push(L, worldIndex, broken.first);
            lua_setfield(L, -2, "bodyA");
        }

        if (broken.second.isValid()) {
            Physics2DBinding::push(L, worldIndex, broken.second);
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

// Creates a body with `createBody({type, x, y, rotation, vx, vy, angularVelocity, linearDamping, angularDamping, gravityScale, lockX, lockY, lockRotation, bullet, continuousThreshold, fastRotation, sleepEnabled, sleepThreshold})`.
int WorldBinding::createBody(lua_State* L) {
    haylen::physics2d::World& world = check(L);
    Physics2DBinding::push(L, 1, world.createBody(readBodyOptions(L, 2)));
    return 1;
}

// Joins two bodies with `createJoint(type, a, b, {ax, ay, bx, by, lower, upper, motorSpeed, maxMotorForce, maxMotorTorque, hertz, ...})`.
int WorldBinding::createJoint(lua_State* L) {
    haylen::physics2d::World& world = check(L);
    const auto type = lua::Stack::read<haylen::physics2d::Joint::Type>(L, 2);
    const haylen::physics2d::Body first = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 3).handle;
    const haylen::physics2d::Body second = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 4).handle;
    Physics2DBinding::push(L, 1, world.createJoint(type, first, second, readJointOptions(L, 5)));
    return 1;
}

std::vector<haylen::physics2d::Body> WorldBinding::readBodies(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<haylen::physics2d::Body> bodies;
    bodies.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        bodies.push_back(lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, -1).handle);
        lua_pop(L, 1);
    }

    return bodies;
}

std::span<float> WorldBinding::readTransformValues(lua_State* L, int index) {
    const std::span<float> values = lua::Userdata::check<haylen::core::FloatBuffer>(L, index).getValues();
    const lua_Integer first = luaL_optinteger(L, index + 1, 1);
    luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first) <= values.size() + 1, index + 1, "the first position is outside the buffer");
    return values.subspan(static_cast<std::size_t>(first - 1));
}

// Copies `x`, `y` and `rotation` of every body into a float buffer with `readTransforms(bodies, buffer[, first[, interpolated]])`, blended between the last two steps by the interpolation of the frame clock when `interpolated` is `true`.
int WorldBinding::readTransforms(lua_State* L) {
    const std::vector<haylen::physics2d::Body> bodies = readBodies(L, 2);
    std::optional<float> blend;
    if (lua_toboolean(L, 5) != 0) {
        blend = static_cast<float>(lua::Runtime::getEngine(L).getClock().getInterpolation());
    }

    check(L).readTransforms(bodies, readTransformValues(L, 3), blend);
    return 0;
}

// Moves every body to the `x`, `y` and `rotation` a float buffer holds with `writeTransforms(bodies, buffer[, first])`.
int WorldBinding::writeTransforms(lua_State* L) {
    const std::vector<haylen::physics2d::Body> bodies = readBodies(L, 2);
    check(L).writeTransforms(bodies, readTransformValues(L, 3));
    return 0;
}

// Bodies also die outside `body:destroy()`, in fractures, assemblies, terrains and the garbage collection of their owners, so every step drops the data of the bodies the world saw go.
void WorldBinding::releaseDestroyedData(lua_State* L, int worldIndex) {
    std::vector<std::uint64_t> destroyed;
    lua::Userdata::check<haylen::physics2d::World>(L, worldIndex).takeDestroyedBodies(destroyed);

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
int WorldBinding::step(lua_State* L) {
    haylen::physics2d::World& world = check(L);
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

    const std::vector<haylen::physics2d::ContactEvent> beginEvents = begins ? world.getContactBegins() : std::vector<haylen::physics2d::ContactEvent>{};
    const std::vector<haylen::physics2d::ContactEvent> endEvents = ends ? world.getContactEnds() : std::vector<haylen::physics2d::ContactEvent>{};
    const std::vector<haylen::physics2d::ContactEvent> hitEvents = hits ? world.getContactHits() : std::vector<haylen::physics2d::ContactEvent>{};
    const std::vector<haylen::physics2d::SensorEvent> sensorBeginEvents = sensorBegins ? world.getSensorBegins() : std::vector<haylen::physics2d::SensorEvent>{};
    const std::vector<haylen::physics2d::SensorEvent> sensorEndEvents = sensorEnds ? world.getSensorEnds() : std::vector<haylen::physics2d::SensorEvent>{};
    const std::vector<haylen::physics2d::World::JointBreak> breakEvents = breaks ? world.getJointBreaks() : std::vector<haylen::physics2d::World::JointBreak>{};

    dispatchContacts(L, 1, "onContactBegin", beginEvents);
    dispatchContacts(L, 1, "onContactEnd", endEvents);
    dispatchContacts(L, 1, "onHit", hitEvents);
    dispatchSensors(L, 1, "onSensorBegin", sensorBeginEvents);
    dispatchSensors(L, 1, "onSensorEnd", sensorEndEvents);
    dispatchJointBreaks(L, 1, breakEvents);
    return 0;
}

int WorldBinding::wakeAll(lua_State* L) {
    check(L).wakeAll();
    return 0;
}

// Destroys the world at once and lets its callbacks and the data of its bodies go, so nothing of it draws, steps or calls back while the collector has not freed the object.
int WorldBinding::destroy(lua_State* L) {
    lua::Userdata::check<haylen::physics2d::World>(L, 1).destroy();
    resetState(L, 1);
    return 0;
}

int WorldBinding::isDestroyed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::physics2d::World>(L, 1).isDestroyed());
    return 1;
}

// Returns `{bodies, awakeBodies, shapes, contacts, joints, islands, stepMilliseconds, collideMilliseconds, solveMilliseconds, continuousMilliseconds, sleepMilliseconds, hookMilliseconds}`.
int WorldBinding::stats(lua_State* L) {
    const haylen::physics2d::World::Stats stats = check(L).getStats();
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
int WorldBinding::stateHash(lua_State* L) {
    const std::vector<haylen::physics2d::Body> bodies = readBodies(L, 2);
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).computeStateHash(bodies)));
    return 1;
}

// Predicts a throw with `predictPath(x, y, vx, vy, {steps, step, radius, gravityScale, linearDamping, category, mask, group})` and returns the list of points and the hit or `nil`.
int WorldBinding::predictPath(lua_State* L) {
    const haylen::physics2d::World& world = check(L);
    haylen::physics2d::PathPredictor::Options options{.step = static_cast<float>(lua::Runtime::getEngine(L).getClock().getFixedStep())};
    if (!lua_isnoneornil(L, 6)) {
        luaL_checktype(L, 6, LUA_TTABLE);
        lua::Table::checkFields(L, 6, {kPredictFields, kFilterFields});
        lua::Table::readField(L, 6, "steps", options.steps);
        lua::Table::readField(L, 6, "step", options.step);
        lua::Table::readField(L, 6, "radius", options.radius);
        lua::Table::readField(L, 6, "gravityScale", options.gravityScale);
        lua::Table::readField(L, 6, "linearDamping", options.linearDamping);
        options.filter = ShapeBinding::readFilter(L, 6, {});
    }

    const haylen::math::Vec2 position{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const haylen::math::Vec2 velocity{lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)};
    const haylen::physics2d::PathPredictor::Path path = haylen::physics2d::PathPredictor::predict(world, position, velocity, options);
    lua::Stack::push(L, path.points);
    if (path.hit) {
        WorldRaycastBinding::pushHit(L, 1, *path.hit);
    } else {
        lua_pushnil(L);
    }

    return 2;
}

int WorldBinding::queryRect(lua_State* L) {
    Physics2DBinding::pushList(L, 1, check(L).queryRect(lua::Stack::read<haylen::math::Rect>(L, 2), readQueryFilter(L, 3)));
    return 1;
}

int WorldBinding::queryCircle(lua_State* L) {
    Physics2DBinding::pushList(L, 1, check(L).queryCircle({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Stack::read<float>(L, 4), readQueryFilter(L, 5)));
    return 1;
}

int WorldBinding::queryPoint(lua_State* L) {
    Physics2DBinding::pushList(L, 1, check(L).queryPoint({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readQueryFilter(L, 4)));
    return 1;
}

int WorldBinding::debugDraw(lua_State* L) {
    check(L).debugDraw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int WorldBinding::getGravity(lua_State* L) {
    lua::Stack::push(L, check(L).getGravity());
    return 1;
}

int WorldBinding::setGravity(lua_State* L) {
    check(L).setGravity(lua::Stack::read<haylen::math::Vec2>(L, 3));
    return 0;
}

int WorldBinding::getBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getBodyCount());
    return 1;
}

int WorldBinding::getAwakeBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getAwakeBodyCount());
    return 1;
}

int WorldBinding::getPixelsPerMeter(lua_State* L) {
    lua::Stack::push(L, check(L).getPixelsPerMeter());
    return 1;
}

int WorldBinding::getThreads(lua_State* L) {
    lua::Stack::push(L, check(L).getThreads());
    return 1;
}

int WorldBinding::isInterpolating(lua_State* L) {
    lua::Stack::push(L, check(L).isInterpolating());
    return 1;
}

int WorldBinding::getSubSteps(lua_State* L) {
    lua::Stack::push(L, check(L).getSubSteps());
    return 1;
}

int WorldBinding::setSubSteps(lua_State* L) {
    check(L).setSubSteps(lua::Stack::read<int>(L, 3));
    return 0;
}

int WorldBinding::isContinuous(lua_State* L) {
    lua::Stack::push(L, check(L).isContinuousEnabled());
    return 1;
}

int WorldBinding::setContinuous(lua_State* L) {
    check(L).setContinuousEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int WorldBinding::isSleepEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isSleepEnabled());
    return 1;
}

int WorldBinding::setSleepEnabled(lua_State* L) {
    check(L).setSleepEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int WorldBinding::getMaxSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxSpeed());
    return 1;
}

int WorldBinding::setMaxSpeed(lua_State* L) {
    check(L).setMaxSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldBinding::getContactHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getContactHertz());
    return 1;
}

int WorldBinding::setContactHertz(lua_State* L) {
    check(L).setContactHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldBinding::getContactDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getContactDampingRatio());
    return 1;
}

int WorldBinding::setContactDampingRatio(lua_State* L) {
    check(L).setContactDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldBinding::getContactPushSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getContactPushSpeed());
    return 1;
}

int WorldBinding::setContactPushSpeed(lua_State* L) {
    check(L).setContactPushSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldBinding::getRestitutionThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getRestitutionThreshold());
    return 1;
}

int WorldBinding::setRestitutionThreshold(lua_State* L) {
    check(L).setRestitutionThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

int WorldBinding::getHitThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getHitThreshold());
    return 1;
}

int WorldBinding::setHitThreshold(lua_State* L) {
    check(L).setHitThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

template <std::size_t Index> int WorldBinding::getCallback(lua_State* L) {
    (void)check(L);
    lua_getiuservalue(L, 1, 1);
    lua_getfield(L, -1, kCallbacks[Index].data());
    return 1;
}

template <std::size_t Index> int WorldBinding::setCallback(lua_State* L) {
    (void)check(L);
    if (!lua_isnil(L, 3)) {
        luaL_checktype(L, 3, LUA_TFUNCTION);
    }

    lua_getiuservalue(L, 1, 1);
    lua_pushvalue(L, 3);
    lua_setfield(L, -2, kCallbacks[Index].data());
    return 0;
}

void WorldBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::physics2d::World>(L).function("createBody", &lua::Binding::native<&createBody>).function("createJoint", &lua::Binding::native<&createJoint>).function("step", &lua::Binding::native<&step>).function("wakeAll", &lua::Binding::native<&wakeAll>).function("destroy", &lua::Binding::native<&destroy>).property("destroyed", &isDestroyed).function("stats", &lua::Binding::native<&stats>).function("stateHash", &lua::Binding::native<&stateHash>).function("predictPath", &lua::Binding::native<&predictPath>).function("readTransforms", &lua::Binding::native<&readTransforms>).function("writeTransforms", &lua::Binding::native<&writeTransforms>).function("raycast", &lua::Binding::native<&WorldRaycastBinding::raycast>).function("raycastAll", &lua::Binding::native<&WorldRaycastBinding::raycastAll>).function("castCircle", &lua::Binding::native<&WorldRaycastBinding::castCircle>).function("castBox", &lua::Binding::native<&WorldRaycastBinding::castBox>).function("castCapsule", &lua::Binding::native<&WorldRaycastBinding::castCapsule>).function("castPolygon", &lua::Binding::native<&WorldRaycastBinding::castPolygon>).function("bounceRay", &lua::Binding::native<&WorldRaycastBinding::bounceRay>).function("rayFan", &lua::Binding::native<&WorldRaycastBinding::rayFan>).function("lineOfSight", &lua::Binding::native<&WorldRaycastBinding::lineOfSight>).function("raycastBatch", &lua::Binding::native<&WorldRaycastBinding::raycastBatch>).function("pick", &lua::Binding::native<&WorldRaycastBinding::pick>).function("debugDrawRays", &lua::Binding::native<&WorldRaycastBinding::debugDrawRays>).property("debugRays", &WorldRaycastBinding::isDebuggingRays, &lua::Binding::native<&WorldRaycastBinding::setDebuggingRays>).function("queryRect", &lua::Binding::native<&queryRect>).function("queryCircle", &lua::Binding::native<&queryCircle>).function("queryPoint", &lua::Binding::native<&queryPoint>).function("debugDraw", &lua::Binding::native<&debugDraw>).property("gravity", &getGravity, &lua::Binding::native<&setGravity>).property("bodyCount", &getBodyCount).property("awakeBodyCount", &getAwakeBodyCount).property("pixelsPerMeter", &getPixelsPerMeter).property("threads", &getThreads).property("interpolate", &isInterpolating).property("subSteps", &getSubSteps, &lua::Binding::native<&setSubSteps>).property("continuous", &isContinuous, &lua::Binding::native<&setContinuous>).property("sleepEnabled", &isSleepEnabled, &lua::Binding::native<&setSleepEnabled>).property("maxSpeed", &getMaxSpeed, &lua::Binding::native<&setMaxSpeed>).property("contactHertz", &getContactHertz, &lua::Binding::native<&setContactHertz>).property("contactDampingRatio", &getContactDampingRatio, &lua::Binding::native<&setContactDampingRatio>).property("contactPushSpeed", &getContactPushSpeed, &lua::Binding::native<&setContactPushSpeed>).property("restitutionThreshold", &getRestitutionThreshold, &lua::Binding::native<&setRestitutionThreshold>).property("hitThreshold", &getHitThreshold, &lua::Binding::native<&setHitThreshold>).property("onContactBegin", &getCallback<0>, &setCallback<0>).property("onContactEnd", &getCallback<1>, &setCallback<1>).property("onHit", &getCallback<2>, &setCallback<2>).property("onSensorBegin", &getCallback<3>, &setCallback<3>).property("onSensorEnd", &getCallback<4>, &setCallback<4>).property("onJointBreak", &getCallback<5>, &setCallback<5>).install();
}

} // namespace haylen::lua::physics2d
