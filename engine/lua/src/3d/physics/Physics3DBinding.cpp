#include "3d/physics/Physics3DBinding.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "3d/physics/BodyBinding.hpp"
#include "3d/physics/ShapeBinding.hpp"
#include "3d/physics/WorldBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/Physics3DPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::physics3d {

// Creates a world with `newWorld({gravity, collisionSteps, velocitySteps, positionSteps, sleepEnabled, timeBeforeSleep, sleepVelocity, speculativeContactDistance, penetrationSlop, restitutionThreshold, maxBodies, maxBodyPairs, maxContactConstraints, threads, temporaryMemory, interpolate, enhancedInternalEdgeRemoval, owner})`, whose settings start from the `physics3d` section of `haylen.json`. The world ends with its owner.
int Physics3DBinding::newWorld(lua_State* L) {
    haylen::plugins::Physics3DPlugin& plugin = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>();
    haylen::physics3d::WorldSettings settings = plugin.getDefaultSettings();
    int owner = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {haylen::physics3d::WorldSettings::kKeys, kOwnerField});
        lua::Table::readField(L, 1, "gravity", settings.gravity);
        lua::Table::readField(L, 1, "collisionSteps", settings.collisionSteps);
        lua::Table::readField(L, 1, "velocitySteps", settings.velocitySteps);
        lua::Table::readField(L, 1, "positionSteps", settings.positionSteps);
        lua::Table::readField(L, 1, "sleepEnabled", settings.sleepEnabled);
        lua::Table::readField(L, 1, "timeBeforeSleep", settings.timeBeforeSleep);
        lua::Table::readField(L, 1, "sleepVelocity", settings.sleepVelocity);
        lua::Table::readField(L, 1, "speculativeContactDistance", settings.speculativeContactDistance);
        lua::Table::readField(L, 1, "penetrationSlop", settings.penetrationSlop);
        lua::Table::readField(L, 1, "restitutionThreshold", settings.restitutionThreshold);
        lua::Table::readField(L, 1, "maxBodies", settings.maxBodies);
        lua::Table::readField(L, 1, "maxBodyPairs", settings.maxBodyPairs);
        lua::Table::readField(L, 1, "maxContactConstraints", settings.maxContactConstraints);
        lua::Table::readField(L, 1, "threads", settings.threads);
        lua::Table::readField(L, 1, "temporaryMemory", settings.temporaryMemory);
        lua::Table::readField(L, 1, "interpolate", settings.interpolate);
        lua::Table::readField(L, 1, "enhancedInternalEdgeRemoval", settings.enhancedInternalEdgeRemoval);
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

    const std::shared_ptr<haylen::physics3d::World> world = plugin.createWorld(settings);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<haylen::plugins::Physics3DPlugin::OwnedWorld>(world));
        lua_remove(L, owner);
    }

    lua::Userdata::emplace<haylen::physics3d::World>(L, world);
    WorldBinding::resetState(L, -1);
    return 1;
}

void Physics3DBinding::pushBody(lua_State* L, int worldIndex, const haylen::physics3d::Body& body) {
    const int owner = lua_absindex(L, worldIndex);
    lua::Userdata::emplace<ScriptedBody>(L, ScriptedBody{.world = lua::Userdata::checkShared<haylen::physics3d::World>(L, owner), .body = body});
    lua_pushvalue(L, owner);
    lua_setiuservalue(L, -2, 1);
}

// Returns the bit of a category of `physics3d.categories` in `haylen.json` with `category(name)`.
int Physics3DBinding::getCategory(lua_State* L) {
    const std::uint32_t bit = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>().getCategories().getBit(luaL_checkstring(L, 1));
    lua_pushinteger(L, static_cast<lua_Integer>(bit));
    return 1;
}

// Returns the names of the categories with `categories()`, in the order of their bits.
int Physics3DBinding::getCategories(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>().getCategories().getNames());
    return 1;
}

// Returns the names of the registered backends with `backends()`, in the order of their names.
int Physics3DBinding::getBackends(lua_State* L) {
    const std::vector<std::string> names = lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>().getBackends().getNames();
    lua::Stack::push(L, names);
    return 1;
}

int Physics3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newWorld", &lua::Binding::native<&newWorld>}, {"backends", &lua::Binding::native<&getBackends>}, {"category", &lua::Binding::native<&getCategory>}, {"categories", &lua::Binding::native<&getCategories>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    ShapeBinding::addFunctions(L);
    return 1;
}

void Physics3DBinding::install(lua_State* L) {
    WorldBinding::install(L);
    BodyBinding::install(L);
    ShapeBinding::install(L);
    lua::Binding::preload(L, "haylen.physics3d", &open);
}

} // namespace haylen::lua::physics3d
