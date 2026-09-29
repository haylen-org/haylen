#include "2d/physics/BodyLua.hpp"

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

Body& BodyLua::check(lua_State* L) {
    ScriptedHandle<Body>& self = lua::Userdata::check<ScriptedHandle<Body>>(L, 1);
    if (!self.handle.isValid()) {
        luaL_error(L, "The body was destroyed.");
    }
    return self.handle;
}

void BodyLua::pushDataSlot(lua_State* L, std::uint64_t id) {
    lua_getiuservalue(L, 1, 1);
    lua_getiuservalue(L, -1, 1);
    lua_getfield(L, -1, "data");
    lua_replace(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
}

std::optional<math::Vec2> BodyLua::readOptionalPoint(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return std::nullopt;
    }
    return math::Vec2{lua::Stack::read<float>(L, index), lua::Stack::read<float>(L, index + 1)};
}

int BodyLua::pushNewShape(lua_State* L, Shape shape) {
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, shape);
    return 1;
}

int BodyLua::pushNewShapes(lua_State* L, const std::vector<Shape>& created) {
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::pushList(L, -1, created);
    return 1;
}

int BodyLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Body>>(L, 1).handle.isValid());
    return 1;
}

int BodyLua::getWorld(lua_State* L) {
    (void)lua::Userdata::check<ScriptedHandle<Body>>(L, 1);
    lua_getiuservalue(L, 1, 1);
    return 1;
}

int BodyLua::getType(lua_State* L) {
    lua::Stack::push(L, check(L).getType());
    return 1;
}

int BodyLua::setType(lua_State* L) {
    check(L).setType(lua::Stack::read<Body::Type>(L, 3));
    return 0;
}

int BodyLua::getX(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().x);
    return 1;
}

int BodyLua::setX(lua_State* L) {
    Body& body = check(L);
    body.setTransform({lua::Stack::read<float>(L, 3), body.getPosition().y}, body.getRotation());
    return 0;
}

int BodyLua::getY(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().y);
    return 1;
}

int BodyLua::setY(lua_State* L) {
    Body& body = check(L);
    body.setTransform({body.getPosition().x, lua::Stack::read<float>(L, 3)}, body.getRotation());
    return 0;
}

int BodyLua::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition());
    return 1;
}

int BodyLua::setPosition(lua_State* L) {
    Body& body = check(L);
    body.setTransform(lua::Stack::read<math::Vec2>(L, 3), body.getRotation());
    return 0;
}

int BodyLua::getRotation(lua_State* L) {
    lua::Stack::push(L, check(L).getRotation());
    return 1;
}

int BodyLua::setRotation(lua_State* L) {
    Body& body = check(L);
    body.setTransform(body.getPosition(), lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyLua::getVelocity(lua_State* L) {
    lua::Stack::push(L, check(L).getVelocity());
    return 1;
}

int BodyLua::setVelocity(lua_State* L) {
    check(L).setVelocity(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int BodyLua::getAngularVelocity(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularVelocity());
    return 1;
}

int BodyLua::setAngularVelocity(lua_State* L) {
    check(L).setAngularVelocity(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyLua::getMass(lua_State* L) {
    lua::Stack::push(L, check(L).getMass());
    return 1;
}

int BodyLua::isAwake(lua_State* L) {
    lua::Stack::push(L, check(L).isAwake());
    return 1;
}

int BodyLua::setAwake(lua_State* L) {
    check(L).setAwake(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyLua::isEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isEnabled());
    return 1;
}

int BodyLua::setEnabled(lua_State* L) {
    check(L).setEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyLua::getLinearDamping(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearDamping());
    return 1;
}

int BodyLua::setLinearDamping(lua_State* L) {
    check(L).setLinearDamping(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyLua::getAngularDamping(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularDamping());
    return 1;
}

int BodyLua::setAngularDamping(lua_State* L) {
    check(L).setAngularDamping(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyLua::getGravityScale(lua_State* L) {
    lua::Stack::push(L, check(L).getGravityScale());
    return 1;
}

int BodyLua::setGravityScale(lua_State* L) {
    check(L).setGravityScale(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyLua::isFixedRotation(lua_State* L) {
    lua::Stack::push(L, check(L).isFixedRotation());
    return 1;
}

int BodyLua::setFixedRotation(lua_State* L) {
    check(L).setFixedRotation(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyLua::isBullet(lua_State* L) {
    lua::Stack::push(L, check(L).isBullet());
    return 1;
}

int BodyLua::setBullet(lua_State* L) {
    check(L).setBullet(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyLua::getData(lua_State* L) {
    const std::uint64_t id = check(L).getId();
    pushDataSlot(L, id);
    lua_gettable(L, -2);
    return 1;
}

int BodyLua::setData(lua_State* L) {
    const std::uint64_t id = check(L).getId();
    pushDataSlot(L, id);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    return 0;
}

int BodyLua::applyForce(lua_State* L) {
    check(L).applyForce({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readOptionalPoint(L, 4));
    return 0;
}

int BodyLua::applyImpulse(lua_State* L) {
    check(L).applyImpulse({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readOptionalPoint(L, 4));
    return 0;
}

int BodyLua::applyTorque(lua_State* L) {
    check(L).applyTorque(lua::Stack::read<float>(L, 2));
    return 0;
}

int BodyLua::applyAngularImpulse(lua_State* L) {
    check(L).applyAngularImpulse(lua::Stack::read<float>(L, 2));
    return 0;
}

int BodyLua::setTransform(lua_State* L) {
    check(L).setTransform({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, static_cast<float>(luaL_optnumber(L, 4, 0.0)));
    return 0;
}

int BodyLua::addBox(lua_State* L) {
    return pushNewShape(L, check(L).addBox({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, ShapeLua::readOptions(L, 4)));
}

int BodyLua::addCircle(lua_State* L) {
    return pushNewShape(L, check(L).addCircle(lua::Stack::read<float>(L, 2), ShapeLua::readOptions(L, 3)));
}

int BodyLua::addCapsule(lua_State* L) {
    return pushNewShape(L, check(L).addCapsule({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}, lua::Stack::read<float>(L, 6), ShapeLua::readOptions(L, 7)));
}

int BodyLua::addSegment(lua_State* L) {
    return pushNewShape(L, check(L).addSegment({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}, ShapeLua::readOptions(L, 6)));
}

int BodyLua::addPolygon(lua_State* L) {
    return pushNewShapes(L, check(L).addPolygon(lua::Stack::read<std::vector<math::Vec2>>(L, 2), ShapeLua::readOptions(L, 3)));
}

int BodyLua::addChain(lua_State* L) {
    return pushNewShapes(L, check(L).addChain(lua::Stack::read<std::vector<math::Vec2>>(L, 2), lua_toboolean(L, 3) != 0, ShapeLua::readOptions(L, 4)));
}

int BodyLua::shapes(lua_State* L) {
    return pushNewShapes(L, check(L).getShapes());
}

int BodyLua::outlines(lua_State* L) {
    const std::vector<Shape::Outline> found = check(L).getOutlines();
    lua_createtable(L, static_cast<int>(found.size()), 0);
    for (std::size_t index = 0; index < found.size(); ++index) {
        ShapeLua::pushOutline(L, found[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int BodyLua::destroy(lua_State* L) {
    ScriptedHandle<Body>& self = lua::Userdata::check<ScriptedHandle<Body>>(L, 1);
    if (!self.handle.isValid()) {
        return 0;
    }
    pushDataSlot(L, self.handle.getId());
    lua_pushnil(L);
    lua_settable(L, -3);
    self.handle.destroy();
    return 0;
}

int BodyLua::equal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Body>>(L, 1).handle == lua::Userdata::check<ScriptedHandle<Body>>(L, 2).handle);
    return 1;
}

void BodyLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Body>>(L).function("addBox", &lua::Binding::native<&addBox>).function("addCircle", &lua::Binding::native<&addCircle>).function("addCapsule", &lua::Binding::native<&addCapsule>).function("addSegment", &lua::Binding::native<&addSegment>).function("addPolygon", &lua::Binding::native<&addPolygon>).function("addChain", &lua::Binding::native<&addChain>).function("shapes", &lua::Binding::native<&shapes>).function("outlines", &lua::Binding::native<&outlines>).function("applyForce", &lua::Binding::native<&applyForce>).function("applyImpulse", &lua::Binding::native<&applyImpulse>).function("applyTorque", &lua::Binding::native<&applyTorque>).function("applyAngularImpulse", &lua::Binding::native<&applyAngularImpulse>).function("setTransform", &lua::Binding::native<&setTransform>).function("destroy", &lua::Binding::native<&destroy>).property("valid", &isValid).property("world", &getWorld).property("type", &lua::Binding::native<&getType>, &lua::Binding::native<&setType>).property("x", &lua::Binding::native<&getX>, &lua::Binding::native<&setX>).property("y", &lua::Binding::native<&getY>, &lua::Binding::native<&setY>).property("position", &lua::Binding::native<&getPosition>, &lua::Binding::native<&setPosition>).property("rotation", &lua::Binding::native<&getRotation>, &lua::Binding::native<&setRotation>).property("velocity", &lua::Binding::native<&getVelocity>, &lua::Binding::native<&setVelocity>).property("angularVelocity", &lua::Binding::native<&getAngularVelocity>, &lua::Binding::native<&setAngularVelocity>).property("mass", &lua::Binding::native<&getMass>).property("awake", &lua::Binding::native<&isAwake>, &lua::Binding::native<&setAwake>).property("enabled", &lua::Binding::native<&isEnabled>, &lua::Binding::native<&setEnabled>).property("linearDamping", &lua::Binding::native<&getLinearDamping>, &lua::Binding::native<&setLinearDamping>).property("angularDamping", &lua::Binding::native<&getAngularDamping>, &lua::Binding::native<&setAngularDamping>).property("gravityScale", &lua::Binding::native<&getGravityScale>, &lua::Binding::native<&setGravityScale>).property("fixedRotation", &lua::Binding::native<&isFixedRotation>, &lua::Binding::native<&setFixedRotation>).property("bullet", &lua::Binding::native<&isBullet>, &lua::Binding::native<&setBullet>).property("data", &lua::Binding::native<&getData>, &lua::Binding::native<&setData>).meta("__eq", &equal).install();
}

} // namespace haylen::physics2d
