#include "3d/physics/WorldBinding.hpp"

#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"

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

// Advances the world by a time in seconds with `step(seconds)`.
int WorldBinding::step(lua_State* L) {
    check(L).step(lua::Stack::read<float>(L, 2));
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
    lua::ClassBuilder<World>(L).function("step", &lua::Binding::native<&step>).function("destroy", &lua::Binding::native<&destroy>).property("destroyed", &isDestroyed).function("stats", &lua::Binding::native<&stats>).function("stateHash", &lua::Binding::native<&stateHash>).property("backend", &getBackend).property("bodyCount", &getBodyCount).property("activeBodyCount", &getActiveBodyCount).property("gravity", &get<&World::getGravity>, &lua::Binding::native<&set<&World::setGravity, haylen::math::Vec3>>).property("collisionSteps", &get<&World::getCollisionSteps>, &lua::Binding::native<&set<&World::setCollisionSteps, int>>).property("velocitySteps", &get<&World::getVelocitySteps>, &lua::Binding::native<&set<&World::setVelocitySteps, int>>).property("positionSteps", &get<&World::getPositionSteps>, &lua::Binding::native<&set<&World::setPositionSteps, int>>).property("sleepEnabled", &get<&World::isSleepEnabled>, &lua::Binding::native<&set<&World::setSleepEnabled, bool>>).property("timeBeforeSleep", &get<&World::getTimeBeforeSleep>, &lua::Binding::native<&set<&World::setTimeBeforeSleep, float>>).property("sleepVelocity", &get<&World::getSleepVelocity>, &lua::Binding::native<&set<&World::setSleepVelocity, float>>).property("speculativeContactDistance", &get<&World::getSpeculativeContactDistance>, &lua::Binding::native<&set<&World::setSpeculativeContactDistance, float>>).property("penetrationSlop", &get<&World::getPenetrationSlop>, &lua::Binding::native<&set<&World::setPenetrationSlop, float>>).property("restitutionThreshold", &get<&World::getRestitutionThreshold>, &lua::Binding::native<&set<&World::setRestitutionThreshold, float>>).property("maxBodies", &getLimit<&WorldSettings::maxBodies>).property("maxBodyPairs", &getLimit<&WorldSettings::maxBodyPairs>).property("maxContactConstraints", &getLimit<&WorldSettings::maxContactConstraints>).property("threads", &getLimit<&WorldSettings::threads>).property("temporaryMemory", &getLimit<&WorldSettings::temporaryMemory>).install();
}

} // namespace haylen::lua::physics3d
