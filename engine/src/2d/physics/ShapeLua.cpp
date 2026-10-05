#include "2d/physics/ShapeLua.hpp"

#include <cstdint>
#include <optional>

#include "2d/physics/Physics2DLua.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

CollisionFilter ShapeLua::readFilter(lua_State* L, int index, CollisionFilter filter) {
    auto category = static_cast<lua_Integer>(filter.category);
    auto mask = static_cast<lua_Integer>(filter.mask);
    lua::Table::readField(L, index, "category", category);
    lua::Table::readField(L, index, "mask", mask);
    lua::Table::readField(L, index, "group", filter.group);
    filter.category = static_cast<std::uint64_t>(category);
    filter.mask = static_cast<std::uint64_t>(mask);
    return filter;
}

Shape::Options ShapeLua::readOptions(lua_State* L, int index) {
    Shape::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kOptionFields});
    lua::Table::readField(L, index, "density", options.density);
    lua::Table::readField(L, index, "friction", options.friction);
    lua::Table::readField(L, index, "restitution", options.restitution);
    lua::Table::readField(L, index, "rollingResistance", options.rollingResistance);
    lua::Table::readField(L, index, "sensor", options.sensor);
    lua::Table::readField(L, index, "offsetX", options.offset.x);
    lua::Table::readField(L, index, "offsetY", options.offset.y);
    lua::Table::readField(L, index, "rotation", options.rotation);
    lua::Table::readField(L, index, "tangentSpeed", options.tangentSpeed);
    lua::Table::readField(L, index, "oneWay", options.oneWay);
    lua::Table::readField(L, index, "contactEvents", options.contactEvents);
    lua::Table::readField(L, index, "hitEvents", options.hitEvents);
    lua::Table::readField(L, index, "sensorEvents", options.sensorEvents);
    options.filter = readFilter(L, index, {});
    return options;
}

void ShapeLua::pushOutline(lua_State* L, const Shape::Outline& outline) {
    lua_createtable(L, 0, 2);
    lua::Stack::push(L, outline.points);
    lua_setfield(L, -2, "points");
    lua::Stack::push(L, outline.closed);
    lua_setfield(L, -2, "closed");
}

Shape& ShapeLua::check(lua_State* L) {
    ScriptedHandle<Shape>& self = lua::Userdata::check<ScriptedHandle<Shape>>(L, 1);
    if (!self.handle.isValid()) {
        luaL_error(L, "The shape was destroyed.");
    }
    return self.handle;
}

int ShapeLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Shape>>(L, 1).handle.isValid());
    return 1;
}

int ShapeLua::getBody(lua_State* L) {
    const Body owner = check(L).getBody();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, owner);
    return 1;
}

int ShapeLua::isSensor(lua_State* L) {
    lua::Stack::push(L, check(L).isSensor());
    return 1;
}

int ShapeLua::getKind(lua_State* L) {
    lua::Stack::push(L, check(L).getKind());
    return 1;
}

int ShapeLua::getPoints(lua_State* L) {
    lua::Stack::push(L, check(L).getPoints());
    return 1;
}

int ShapeLua::getWorldPoints(lua_State* L) {
    lua::Stack::push(L, check(L).getWorldPoints());
    return 1;
}

int ShapeLua::getRadius(lua_State* L) {
    lua::Stack::push(L, check(L).getRadius());
    return 1;
}

int ShapeLua::outline(lua_State* L) {
    pushOutline(L, check(L).getOutline());
    return 1;
}

int ShapeLua::getBounds(lua_State* L) {
    lua::Stack::push(L, check(L).getBounds());
    return 1;
}

int ShapeLua::getCategory(lua_State* L) {
    lua::Stack::push(L, static_cast<lua_Integer>(check(L).getFilter().category));
    return 1;
}

int ShapeLua::setCategory(lua_State* L) {
    Shape& shape = check(L);
    CollisionFilter filter = shape.getFilter();
    filter.category = static_cast<std::uint64_t>(luaL_checkinteger(L, 3));
    shape.setFilter(filter);
    return 0;
}

int ShapeLua::getMask(lua_State* L) {
    lua::Stack::push(L, static_cast<lua_Integer>(check(L).getFilter().mask));
    return 1;
}

int ShapeLua::setMask(lua_State* L) {
    Shape& shape = check(L);
    CollisionFilter filter = shape.getFilter();
    filter.mask = static_cast<std::uint64_t>(luaL_checkinteger(L, 3));
    shape.setFilter(filter);
    return 0;
}

int ShapeLua::getGroup(lua_State* L) {
    lua::Stack::push(L, check(L).getFilter().group);
    return 1;
}

int ShapeLua::setGroup(lua_State* L) {
    Shape& shape = check(L);
    CollisionFilter filter = shape.getFilter();
    filter.group = lua::Stack::read<int>(L, 3);
    shape.setFilter(filter);
    return 0;
}

int ShapeLua::getTangentSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getTangentSpeed());
    return 1;
}

int ShapeLua::setTangentSpeed(lua_State* L) {
    check(L).setTangentSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int ShapeLua::getOneWay(lua_State* L) {
    lua::Stack::push(L, check(L).getOneWay());
    return 1;
}

int ShapeLua::setOneWay(lua_State* L) {
    check(L).setOneWay(lua::Stack::read<std::optional<math::Vec2>>(L, 3));
    return 0;
}

int ShapeLua::getFriction(lua_State* L) {
    lua::Stack::push(L, check(L).getFriction());
    return 1;
}

int ShapeLua::setFriction(lua_State* L) {
    check(L).setFriction(lua::Stack::read<float>(L, 3));
    return 0;
}

int ShapeLua::getRestitution(lua_State* L) {
    lua::Stack::push(L, check(L).getRestitution());
    return 1;
}

int ShapeLua::setRestitution(lua_State* L) {
    check(L).setRestitution(lua::Stack::read<float>(L, 3));
    return 0;
}

int ShapeLua::getDensity(lua_State* L) {
    lua::Stack::push(L, check(L).getDensity());
    return 1;
}

int ShapeLua::setDensity(lua_State* L) {
    check(L).setDensity(lua::Stack::read<float>(L, 3));
    return 0;
}

int ShapeLua::getRollingResistance(lua_State* L) {
    lua::Stack::push(L, check(L).getRollingResistance());
    return 1;
}

int ShapeLua::setRollingResistance(lua_State* L) {
    check(L).setRollingResistance(lua::Stack::read<float>(L, 3));
    return 0;
}

int ShapeLua::hasContactEvents(lua_State* L) {
    lua::Stack::push(L, check(L).hasContactEvents());
    return 1;
}

int ShapeLua::setContactEvents(lua_State* L) {
    check(L).setContactEvents(lua::Stack::read<bool>(L, 3));
    return 0;
}

int ShapeLua::hasHitEvents(lua_State* L) {
    lua::Stack::push(L, check(L).hasHitEvents());
    return 1;
}

int ShapeLua::setHitEvents(lua_State* L) {
    check(L).setHitEvents(lua::Stack::read<bool>(L, 3));
    return 0;
}

int ShapeLua::hasSensorEvents(lua_State* L) {
    lua::Stack::push(L, check(L).hasSensorEvents());
    return 1;
}

int ShapeLua::setSensorEvents(lua_State* L) {
    check(L).setSensorEvents(lua::Stack::read<bool>(L, 3));
    return 0;
}

int ShapeLua::overlaps(lua_State* L) {
    const std::vector<Shape> found = check(L).getOverlaps();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::pushList(L, -1, found);
    return 1;
}

int ShapeLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Shape>>(L, 1).handle.destroy();
    return 0;
}

int ShapeLua::equal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Shape>>(L, 1).handle == lua::Userdata::check<ScriptedHandle<Shape>>(L, 2).handle);
    return 1;
}

void ShapeLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Shape>>(L).function("destroy", &lua::Binding::native<&destroy>).function("outline", &lua::Binding::native<&outline>).function("overlaps", &lua::Binding::native<&overlaps>).property("friction", &lua::Binding::native<&getFriction>, &lua::Binding::native<&setFriction>).property("restitution", &lua::Binding::native<&getRestitution>, &lua::Binding::native<&setRestitution>).property("density", &lua::Binding::native<&getDensity>, &lua::Binding::native<&setDensity>).property("rollingResistance", &lua::Binding::native<&getRollingResistance>, &lua::Binding::native<&setRollingResistance>).property("contactEvents", &lua::Binding::native<&hasContactEvents>, &lua::Binding::native<&setContactEvents>).property("hitEvents", &lua::Binding::native<&hasHitEvents>, &lua::Binding::native<&setHitEvents>).property("sensorEvents", &lua::Binding::native<&hasSensorEvents>, &lua::Binding::native<&setSensorEvents>).property("valid", &isValid).property("kind", &lua::Binding::native<&getKind>).property("points", &lua::Binding::native<&getPoints>).property("worldPoints", &lua::Binding::native<&getWorldPoints>).property("radius", &lua::Binding::native<&getRadius>).property("body", &lua::Binding::native<&getBody>).property("sensor", &lua::Binding::native<&isSensor>).property("bounds", &lua::Binding::native<&getBounds>).property("category", &lua::Binding::native<&getCategory>, &lua::Binding::native<&setCategory>).property("mask", &lua::Binding::native<&getMask>, &lua::Binding::native<&setMask>).property("group", &lua::Binding::native<&getGroup>, &lua::Binding::native<&setGroup>).property("tangentSpeed", &lua::Binding::native<&getTangentSpeed>, &lua::Binding::native<&setTangentSpeed>).property("oneWay", &lua::Binding::native<&getOneWay>, &lua::Binding::native<&setOneWay>).meta("__eq", &equal).install();
}

} // namespace haylen::physics2d
