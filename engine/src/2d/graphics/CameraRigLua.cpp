#include "2d/graphics/CameraRigLua.hpp"

#include <lua.hpp>

#include <utility>
#include <vector>

#include "2d/graphics/CameraLua.hpp"
#include "2d/graphics/StrokeLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

// A zoom is one number for both axes or a vector.
math::Vec2 CameraRigLua::readZoom(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        const auto zoom = static_cast<float>(lua_tonumber(L, index));
        return {zoom, zoom};
    }
    return lua::Stack::read<math::Vec2>(L, index);
}

CameraRig::Zone CameraRigLua::readZone(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kZoneFields});
    CameraRig::Zone zone;
    lua::Table::readField(L, index, "name", zone.name);
    lua::Table::readField(L, index, "area", zone.area);
    lua::Table::readField(L, index, "limits", zone.limits);
    lua::Table::readField(L, index, "offset", zone.offset);
    lua::Table::readField(L, index, "priority", zone.priority);
    if (lua_getfield(L, index, "zoom") != LUA_TNIL) {
        zone.zoom = readZoom(L, lua_gettop(L));
    }
    lua_pop(L, 1);
    return zone;
}

void CameraRigLua::readRail(lua_State* L, int index, CameraRig& rig) {
    if (lua_isnoneornil(L, index)) {
        rig.setRail({});
        return;
    }
    std::vector<math::Vec2> points;
    StrokeLua::readPoints(L, index, points);
    rig.setRail(std::move(points));
}

int CameraRigLua::create(lua_State* L) {
    CameraRig::Options options;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kOptionFields});
        lua::Table::readField(L, 1, "limits", options.limits);
        lua::Table::readField(L, 1, "blendTime", options.blendTime);
        if (lua_getfield(L, 1, "zoom") != LUA_TNIL) {
            options.zoom = readZoom(L, lua_gettop(L));
        }
        lua_pop(L, 1);
    }
    auto rig = std::make_shared<CameraRig>(options);
    if (!lua_isnoneornil(L, 1)) {
        if (lua_getfield(L, 1, "zones") != LUA_TNIL) {
            luaL_checktype(L, -1, LUA_TTABLE);
            const int list = lua_gettop(L);
            for (lua_Integer place = 1; place <= luaL_len(L, list); ++place) {
                lua_rawgeti(L, list, place);
                rig->addZone(readZone(L, lua_gettop(L)));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "rail");
        readRail(L, lua_gettop(L), *rig);
        lua_pop(L, 1);
    }
    lua::Userdata::emplace<CameraRig>(L, std::move(rig));
    return 1;
}

int CameraRigLua::addZone(lua_State* L) {
    lua::Userdata::check<CameraRig>(L, 1).addZone(readZone(L, 2));
    return 0;
}

int CameraRigLua::removeZone(lua_State* L) {
    lua::Userdata::check<CameraRig>(L, 1).removeZone(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int CameraRigLua::clearZones(lua_State* L) {
    lua::Userdata::check<CameraRig>(L, 1).clearZones();
    return 0;
}

int CameraRigLua::setRail(lua_State* L) {
    readRail(L, 2, lua::Userdata::check<CameraRig>(L, 1));
    return 0;
}

int CameraRigLua::project(lua_State* L) {
    const math::Vec2 point = lua::Userdata::check<CameraRig>(L, 1).project({static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3))});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

// Moves the camera with `update(camera, x, y, dt)`, measuring the view with the visible design area like `camera:follow`.
int CameraRigLua::update(lua_State* L) {
    CameraRig& rig = lua::Userdata::check<CameraRig>(L, 1);
    Camera& camera = lua::Userdata::check<Camera>(L, 2);
    const math::Vec2 target{static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4))};
    rig.update(camera, target, CameraLua::readSeconds(L, 5), CameraLua::getScreen(L));
    return 0;
}

int CameraRigLua::zone(lua_State* L) {
    const CameraRig::Zone* current = lua::Userdata::check<CameraRig>(L, 1).getZone();
    if (current == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, current->name);
    return 1;
}

int CameraRigLua::zoneCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<CameraRig>(L, 1).getZones().size());
    return 1;
}

int CameraRigLua::getBlendTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<CameraRig>(L, 1).getOptions().blendTime);
    return 1;
}

int CameraRigLua::setBlendTime(lua_State* L) {
    CameraRig& rig = lua::Userdata::check<CameraRig>(L, 1);
    CameraRig::Options options = rig.getOptions();
    options.blendTime = lua::Stack::read<float>(L, 3);
    rig.setOptions(options);
    return 0;
}

void CameraRigLua::install(lua_State* L) {
    lua::ClassBuilder<CameraRig>(L).function("addZone", &lua::Binding::native<&addZone>).function("removeZone", &lua::Binding::native<&removeZone>).function("clearZones", &lua::Binding::native<&clearZones>).function("setRail", &lua::Binding::native<&setRail>).function("project", &lua::Binding::native<&project>).function("update", &lua::Binding::native<&update>).property("zone", &zone).property("zoneCount", &zoneCount).property("blendTime", &getBlendTime, &lua::Binding::native<&setBlendTime>).install();
}

} // namespace haylen::graphics2d
