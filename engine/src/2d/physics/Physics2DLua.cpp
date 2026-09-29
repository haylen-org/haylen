#include "2d/physics/Physics2DLua.hpp"

#include "2d/physics/AssemblyLua.hpp"
#include "2d/physics/BodyLua.hpp"
#include "2d/physics/DestructionLua.hpp"
#include "2d/physics/FluidLua.hpp"
#include "2d/physics/JointLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "2d/physics/WorldLua.hpp"
#include "2d/physics/WorldRaycastLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

// Creates a world with newWorld({gravity = {0, 980}, pixelsPerMeter = 64, subSteps = 4}).
int Physics2DLua::newWorld(lua_State* L) {
    World::Settings settings;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kWorldFields});
        lua::Table::readField(L, 1, "gravity", settings.gravity);
        lua::Table::readField(L, 1, "pixelsPerMeter", settings.pixelsPerMeter);
        lua::Table::readField(L, 1, "subSteps", settings.subSteps);
    }
    lua::Userdata::emplace<World>(L, std::make_shared<World>(settings));
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
    DestructionLua::install(L);
    FluidLua::install(L);
    WorldRaycastLua::install(L);
    lua::Binding::preload(L, "haylen.physics2d", &open);
}

} // namespace haylen::physics2d
