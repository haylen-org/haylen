#include "2d/graphics/CameraRigBinding.hpp"

#include <lua.hpp>

#include <utility>
#include <vector>

#include "2d/graphics/CameraBinding.hpp"
#include "2d/graphics/StrokeBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

// A zoom is one number for both axes or a vector.
haylen::math::Vec2 CameraRigBinding::readZoom(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        const auto zoom = lua::Stack::read<float>(L, index);
        return {zoom, zoom};
    }

    return lua::Stack::read<haylen::math::Vec2>(L, index);
}

haylen::graphics2d::CameraRig::Zone CameraRigBinding::readZone(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kZoneFields});
    haylen::graphics2d::CameraRig::Zone zone;
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

void CameraRigBinding::readRail(lua_State* L, int index, haylen::graphics2d::CameraRig& rig) {
    if (lua_isnoneornil(L, index)) {
        rig.setRail({});
        return;
    }

    std::vector<haylen::math::Vec2> points;
    StrokeBinding::readPoints(L, index, points);
    rig.setRail(std::move(points));
}

int CameraRigBinding::create(lua_State* L) {
    haylen::graphics2d::CameraRig::Options options;
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

    auto rig = std::make_shared<haylen::graphics2d::CameraRig>(options);
    if (!lua_isnoneornil(L, 1)) {
        if (lua_getfield(L, 1, "zones") != LUA_TNIL) {
            lua::Table::requireType(L, "zones", LUA_TTABLE);
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

    lua::Userdata::emplace<haylen::graphics2d::CameraRig>(L, std::move(rig));
    return 1;
}

int CameraRigBinding::addZone(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).addZone(readZone(L, 2));
    return 0;
}

int CameraRigBinding::removeZone(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).removeZone(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int CameraRigBinding::clearZones(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).clearZones();
    return 0;
}

int CameraRigBinding::setRail(lua_State* L) {
    readRail(L, 2, lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1));
    return 0;
}

int CameraRigBinding::project(lua_State* L) {
    const haylen::math::Vec2 point = lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).project({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

// Moves the camera with `update(camera, x, y, dt)`, measuring the view with the visible design area like `camera:follow`.
int CameraRigBinding::update(lua_State* L) {
    haylen::graphics2d::CameraRig& rig = lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1);
    haylen::graphics2d::Camera& camera = lua::Userdata::check<haylen::graphics2d::Camera>(L, 2);
    const haylen::math::Vec2 target{lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    rig.update(camera, target, CameraBinding::readSeconds(L, 5), CameraBinding::getScreen(L));
    return 0;
}

int CameraRigBinding::zone(lua_State* L) {
    const haylen::graphics2d::CameraRig::Zone* current = lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).getZone();
    if (current == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, current->name);
    return 1;
}

int CameraRigBinding::zoneCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).getZones().size());
    return 1;
}

int CameraRigBinding::getBlendTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1).getOptions().blendTime);
    return 1;
}

int CameraRigBinding::setBlendTime(lua_State* L) {
    haylen::graphics2d::CameraRig& rig = lua::Userdata::check<haylen::graphics2d::CameraRig>(L, 1);
    haylen::graphics2d::CameraRig::Options options = rig.getOptions();
    options.blendTime = lua::Stack::read<float>(L, 3);
    rig.setOptions(options);
    return 0;
}

void CameraRigBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::CameraRig>(L).function("addZone", &lua::Binding::native<&addZone>).function("removeZone", &lua::Binding::native<&removeZone>).function("clearZones", &lua::Binding::native<&clearZones>).function("setRail", &lua::Binding::native<&setRail>).function("project", &lua::Binding::native<&project>).function("update", &lua::Binding::native<&update>).property("zone", &zone).property("zoneCount", &zoneCount).property("blendTime", &getBlendTime, &lua::Binding::native<&setBlendTime>).install();
}

} // namespace haylen::lua::graphics2d
