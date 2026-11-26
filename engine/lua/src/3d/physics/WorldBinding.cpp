#include "3d/physics/WorldBinding.hpp"

#include <cstdint>

#include "3d/physics/BodyBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"

namespace haylen::lua::physics3d {

using haylen::physics3d::World;
using haylen::physics3d::WorldSettings;

World& WorldBinding::check(lua_State* L) {
    World& world = lua::Userdata::check<World>(L, 1);
    if (world.isDestroyed()) {
        luaL_error(L, "The 3D physics world was destroyed.");
    }

    return world;
}

void WorldBinding::resetState(lua_State* L, int index) {
    const int world = lua_absindex(L, index);
    lua_createtable(L, 0, 1);
    lua_newtable(L);
    lua_setfield(L, -2, "data");
    lua_setiuservalue(L, world, 1);
}

// Bodies also go with their world and the garbage collection of their owners, so every step drops the data of the bodies the world saw go.
void WorldBinding::releaseDestroyedData(lua_State* L, int worldIndex) {
    std::vector<std::uint32_t> destroyed;
    lua::Userdata::check<World>(L, worldIndex).takeDestroyedBodies(destroyed);
    if (destroyed.empty()) {
        return;
    }

    lua_getiuservalue(L, worldIndex, 1);
    lua_getfield(L, -1, "data");
    for (const std::uint32_t id : destroyed) {
        lua_pushnil(L);
        lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    }

    lua_pop(L, 2);
}

// Releases the data of destroyed bodies and advances the world by a time in seconds with `step(seconds)`.
int WorldBinding::step(lua_State* L) {
    World& world = check(L);
    releaseDestroyedData(L, 1);
    world.step(lua::Stack::read<float>(L, 2));
    return 0;
}

// Makes a body with `createBody({shape, ...})`.
int WorldBinding::createBody(lua_State* L) {
    World& world = check(L);
    const haylen::physics3d::Body body = world.createBody(BodyBinding::readOptions(L, 2));
    Physics3DBinding::pushBody(L, 1, body);
    return 1;
}

std::vector<haylen::physics3d::Body> WorldBinding::readBodies(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const auto count = static_cast<std::size_t>(lua_rawlen(L, index));
    std::vector<haylen::physics3d::Body> bodies;
    bodies.reserve(count);
    for (std::size_t item = 1; item <= count; ++item) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(item));
        bodies.push_back(lua::Userdata::check<ScriptedBody>(L, -1).body);
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

// Copies the position and rotation of every body, seven floats each, into a float buffer with `readTransforms(bodies, buffer[, first[, interpolated]])`, blended between the last two steps by the interpolation of the frame clock when `interpolated` is `true`.
int WorldBinding::readTransforms(lua_State* L) {
    const World& world = check(L);
    const std::vector<haylen::physics3d::Body> bodies = readBodies(L, 2);
    std::optional<float> blend;
    if (lua_toboolean(L, 5) != 0) {
        blend = static_cast<float>(lua::Runtime::getEngine(L).getClock().getInterpolation());
    }

    world.readTransforms(bodies, readTransformValues(L, 3), blend);
    return 0;
}

// Moves every body to the position and rotation that a float buffer holds with `writeTransforms(bodies, buffer[, first])`.
int WorldBinding::writeTransforms(lua_State* L) {
    World& world = check(L);
    const std::vector<haylen::physics3d::Body> bodies = readBodies(L, 2);
    world.writeTransforms(bodies, readTransformValues(L, 3));
    return 0;
}

int WorldBinding::isInterpolating(lua_State* L) {
    lua::Stack::push(L, check(L).isInterpolating());
    return 1;
}

int WorldBinding::hasEnhancedInternalEdgeRemoval(lua_State* L) {
    lua::Stack::push(L, check(L).hasEnhancedInternalEdgeRemoval());
    return 1;
}

int WorldBinding::setEnhancedInternalEdgeRemoval(lua_State* L) {
    check(L).setEnhancedInternalEdgeRemoval(lua::Stack::read<bool>(L, 3));
    return 0;
}

// Ends the world at once with everything it holds, before the collector frees the object.
int WorldBinding::destroy(lua_State* L) {
    lua::Userdata::check<World>(L, 1).destroy();
    return 0;
}

int WorldBinding::isDestroyed(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<World>(L, 1).isDestroyed());
    return 1;
}

// Returns `{bodies, activeBodies, stepThreads, stepMilliseconds, temporaryMemoryPeak}`.
int WorldBinding::stats(lua_State* L) {
    const haylen::physics3d::WorldStats stats = check(L).getStats();
    lua_createtable(L, 0, 5);
    lua::Stack::push(L, stats.bodies);
    lua_setfield(L, -2, "bodies");
    lua::Stack::push(L, stats.activeBodies);
    lua_setfield(L, -2, "activeBodies");
    lua::Stack::push(L, stats.stepThreads);
    lua_setfield(L, -2, "stepThreads");
    lua::Stack::push(L, stats.stepMilliseconds);
    lua_setfield(L, -2, "stepMilliseconds");
    lua_pushinteger(L, static_cast<lua_Integer>(stats.temporaryMemoryPeak));
    lua_setfield(L, -2, "temporaryMemoryPeak");
    return 1;
}

// Returns the hash of the whole state of the world with `stateHash()`, as an integer.
int WorldBinding::stateHash(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).computeStateHash()));
    return 1;
}

int WorldBinding::getBackend(lua_State* L) {
    lua::Stack::push(L, check(L).getBackendName());
    return 1;
}

int WorldBinding::getBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getStats().bodies);
    return 1;
}

int WorldBinding::getActiveBodyCount(lua_State* L) {
    lua::Stack::push(L, check(L).getStats().activeBodies);
    return 1;
}

void WorldBinding::install(lua_State* L) {
    lua::ClassBuilder<World>(L).function("step", &lua::Binding::native<&step>).function("createBody", &lua::Binding::native<&createBody>).function("readTransforms", &lua::Binding::native<&readTransforms>).function("writeTransforms", &lua::Binding::native<&writeTransforms>).property("interpolate", &lua::Binding::native<&isInterpolating>).property("enhancedInternalEdgeRemoval", &lua::Binding::native<&hasEnhancedInternalEdgeRemoval>, &lua::Binding::native<&setEnhancedInternalEdgeRemoval>).function("destroy", &lua::Binding::native<&destroy>).property("destroyed", &isDestroyed).function("stats", &lua::Binding::native<&stats>).function("stateHash", &lua::Binding::native<&stateHash>).property("backend", &getBackend).property("bodyCount", &getBodyCount).property("activeBodyCount", &getActiveBodyCount).property("gravity", &get<&World::getGravity>, &lua::Binding::native<&set<&World::setGravity, haylen::math::Vec3>>).property("collisionSteps", &get<&World::getCollisionSteps>, &lua::Binding::native<&set<&World::setCollisionSteps, int>>).property("velocitySteps", &get<&World::getVelocitySteps>, &lua::Binding::native<&set<&World::setVelocitySteps, int>>).property("positionSteps", &get<&World::getPositionSteps>, &lua::Binding::native<&set<&World::setPositionSteps, int>>).property("sleepEnabled", &get<&World::isSleepEnabled>, &lua::Binding::native<&set<&World::setSleepEnabled, bool>>).property("timeBeforeSleep", &get<&World::getTimeBeforeSleep>, &lua::Binding::native<&set<&World::setTimeBeforeSleep, float>>).property("sleepVelocity", &get<&World::getSleepVelocity>, &lua::Binding::native<&set<&World::setSleepVelocity, float>>).property("speculativeContactDistance", &get<&World::getSpeculativeContactDistance>, &lua::Binding::native<&set<&World::setSpeculativeContactDistance, float>>).property("penetrationSlop", &get<&World::getPenetrationSlop>, &lua::Binding::native<&set<&World::setPenetrationSlop, float>>).property("restitutionThreshold", &get<&World::getRestitutionThreshold>, &lua::Binding::native<&set<&World::setRestitutionThreshold, float>>).property("maxBodies", &getLimit<&WorldSettings::maxBodies>).property("maxBodyPairs", &getLimit<&WorldSettings::maxBodyPairs>).property("maxContactConstraints", &getLimit<&WorldSettings::maxContactConstraints>).property("threads", &getLimit<&WorldSettings::threads>).property("temporaryMemory", &getLimit<&WorldSettings::temporaryMemory>).install();
}

} // namespace haylen::lua::physics3d
