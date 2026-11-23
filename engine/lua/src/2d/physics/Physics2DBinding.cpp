#include "2d/physics/Physics2DBinding.hpp"

#include <algorithm>

#include "2d/physics/AssemblyBinding.hpp"
#include "2d/physics/BodyBinding.hpp"
#include "2d/physics/DestructionBinding.hpp"
#include "2d/physics/FluidBinding.hpp"
#include "2d/physics/ForceFieldBinding.hpp"
#include "2d/physics/GrabberBinding.hpp"
#include "2d/physics/JointBinding.hpp"
#include "2d/physics/MoverBinding.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "2d/physics/VehicleBinding.hpp"
#include "2d/physics/WorldBinding.hpp"
#include "2d/physics/WorldRaycastBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/Physics2DPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::physics2d {

void Physics2DBinding::setManifold(lua_State* L, const haylen::physics2d::ContactManifold& manifold) {
    const haylen::math::Vec2 point = manifold.getPoint();
    // clang-format off
    const auto setNumber = [L](const char* name, float value) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    };
    // clang-format on
    setNumber("x", point.x);
    setNumber("y", point.y);
    setNumber("normalX", manifold.normal.x);
    setNumber("normalY", manifold.normal.y);
    setNumber("speed", manifold.approachSpeed);
    setNumber("relativeVelocityX", manifold.relativeVelocity.x);
    setNumber("relativeVelocityY", manifold.relativeVelocity.y);
    setNumber("force", manifold.force);

    lua_createtable(L, static_cast<int>(manifold.pointCount), 0);
    for (std::size_t index = 0; index < manifold.pointCount; ++index) {
        const haylen::physics2d::ContactPoint& contact = manifold.points[index];
        lua_createtable(L, 0, 6);
        setNumber("x", contact.point.x);
        setNumber("y", contact.point.y);
        setNumber("separation", contact.separation);
        setNumber("normalImpulse", contact.normalImpulse);
        setNumber("tangentImpulse", contact.tangentImpulse);
        setNumber("approachSpeed", contact.approachSpeed);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -2, "points");
}

// Creates a world with `newWorld({gravity = {0, 980}, pixelsPerMeter = 64, subSteps = 4, threads, continuous, sleepEnabled, interpolate, owner, ...})`, stepped on the job system of the engine. The debug drawing shows the world until its owner ends.
int Physics2DBinding::newWorld(lua_State* L) {
    haylen::core::JobSystem& jobs = lua::Runtime::getEngine(L).getJobs();
    haylen::physics2d::World::Settings settings{.threads = std::min(static_cast<int>(jobs.getWorkerCount()), kDefaultThreads)};
    int owner = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kWorldFields});
        lua::Table::readField(L, 1, "gravity", settings.gravity);
        lua::Table::readField(L, 1, "pixelsPerMeter", settings.pixelsPerMeter);
        lua::Table::readField(L, 1, "subSteps", settings.subSteps);
        lua::Table::readField(L, 1, "threads", settings.threads);
        lua::Table::readField(L, 1, "continuous", settings.continuous);
        lua::Table::readField(L, 1, "sleepEnabled", settings.sleepEnabled);
        lua::Table::readField(L, 1, "interpolate", settings.interpolate);
        lua::Table::readField(L, 1, "contactHertz", settings.contactHertz);
        lua::Table::readField(L, 1, "contactDampingRatio", settings.contactDampingRatio);
        lua::Table::readField(L, 1, "contactPushSpeed", settings.contactPushSpeed);
        lua::Table::readField(L, 1, "maxSpeed", settings.maxSpeed);
        lua::Table::readField(L, 1, "restitutionThreshold", settings.restitutionThreshold);
        lua::Table::readField(L, 1, "hitThreshold", settings.hitThreshold);
        if (lua_getfield(L, 1, "owner") == LUA_TNIL) {
            lua_pop(L, 1);
        } else {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    const std::shared_ptr<haylen::physics2d::World> world = std::make_shared<haylen::physics2d::World>(settings, &jobs);
    const std::shared_ptr<haylen::plugins::Physics2DPlugin::TrackedWorld> tracked = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics2DPlugin>().track(world);
    if (owner != 0) {
        lua::Owners::add(L, owner, tracked);
        lua_remove(L, owner);
    }

    lua::Userdata::emplace<haylen::physics2d::World>(L, world);
    WorldBinding::resetState(L, -1);
    return 1;
}

int Physics2DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newWorld", &lua::Binding::native<&newWorld>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    AssemblyBinding::addFunctions(L);
    VehicleBinding::addFunctions(L);
    MoverBinding::addFunctions(L);
    GrabberBinding::addFunctions(L);
    ForceFieldBinding::addFunctions(L);
    DestructionBinding::addFunctions(L);
    FluidBinding::addFunctions(L);
    WorldRaycastBinding::addFunctions(L);
    return 1;
}

void Physics2DBinding::install(lua_State* L) {
    WorldBinding::install(L);
    BodyBinding::install(L);
    ShapeBinding::install(L);
    JointBinding::install(L);
    AssemblyBinding::install(L);
    VehicleBinding::install(L);
    MoverBinding::install(L);
    GrabberBinding::install(L);
    ForceFieldBinding::install(L);
    DestructionBinding::install(L);
    FluidBinding::install(L);
    WorldRaycastBinding::install(L);
    lua::Binding::preload(L, "haylen.physics2d", &open);
}

} // namespace haylen::lua::physics2d
