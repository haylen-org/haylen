#include "2d/physics/Physics2DLua.hpp"

#include <algorithm>

#include "2d/physics/AssemblyLua.hpp"
#include "2d/physics/BodyLua.hpp"
#include "2d/physics/DestructionLua.hpp"
#include "2d/physics/FluidLua.hpp"
#include "2d/physics/ForceFieldLua.hpp"
#include "2d/physics/GrabberLua.hpp"
#include "2d/physics/JointLua.hpp"
#include "2d/physics/MoverLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "2d/physics/VehicleLua.hpp"
#include "2d/physics/WorldLua.hpp"
#include "2d/physics/WorldRaycastLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "plugins/Physics2DPlugin.hpp"

namespace haylen::physics2d {

// Creates a world with `newWorld({gravity = {0, 980}, pixelsPerMeter = 64, subSteps = 4, threads, continuous, sleepEnabled, interpolate, ...})`, stepped on the job system of the engine.
int Physics2DLua::newWorld(lua_State* L) {
    core::JobSystem& jobs = lua::Runtime::getEngine(L).getJobs();
    World::Settings settings{.threads = std::min(static_cast<int>(jobs.getWorkerCount()), kDefaultThreads)};
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
    }
    const std::shared_ptr<World> world = std::make_shared<World>(settings, &jobs);
    lua::Runtime::getEngine(L).getPlugin<plugins::Physics2DPlugin>().track(world);
    lua::Userdata::emplace<World>(L, world);
    lua_createtable(L, 0, 6);
    lua_newtable(L);
    lua_setfield(L, -2, "data");
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int Physics2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newWorld", &lua::Binding::native<&newWorld>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    AssemblyLua::addFunctions(L);
    VehicleLua::addFunctions(L);
    MoverLua::addFunctions(L);
    GrabberLua::addFunctions(L);
    ForceFieldLua::addFunctions(L);
    DestructionLua::addFunctions(L);
    FluidLua::addFunctions(L);
    WorldRaycastLua::addFunctions(L);
    return 1;
}

void Physics2DLua::install(lua_State* L) {
    WorldLua::install(L);
    BodyLua::install(L);
    ShapeLua::install(L);
    JointLua::install(L);
    AssemblyLua::install(L);
    VehicleLua::install(L);
    MoverLua::install(L);
    GrabberLua::install(L);
    ForceFieldLua::install(L);
    DestructionLua::install(L);
    FluidLua::install(L);
    WorldRaycastLua::install(L);
    lua::Binding::preload(L, "haylen.physics2d", &open);
}

} // namespace haylen::physics2d
