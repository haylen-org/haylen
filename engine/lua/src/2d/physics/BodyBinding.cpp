#include "2d/physics/BodyBinding.hpp"

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::physics2d {

haylen::physics2d::Body& BodyBinding::check(lua_State* L) {
    ScriptedHandle<haylen::physics2d::Body>& self = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1);

    if (self.world->isDestroyed()) {
        luaL_error(L, "The physics world of the body was destroyed.");
    }

    if (!self.handle.isValid()) {
        luaL_error(L, "The body was destroyed.");
    }

    return self.handle;
}

void BodyBinding::pushDataSlot(lua_State* L, std::uint64_t id) {
    lua_getiuservalue(L, 1, 1);
    lua_getiuservalue(L, -1, 1);
    lua_getfield(L, -1, "data");
    lua_replace(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
}

std::optional<haylen::math::Vec2> BodyBinding::readOptionalPoint(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return std::nullopt;
    }

    return haylen::math::Vec2{lua::Stack::read<float>(L, index), lua::Stack::read<float>(L, index + 1)};
}

int BodyBinding::pushNewShape(lua_State* L, haylen::physics2d::Shape shape) {
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, shape);
    return 1;
}

int BodyBinding::pushNewShapes(lua_State* L, const std::vector<haylen::physics2d::Shape>& created) {
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::pushList(L, -1, created);
    return 1;
}

int BodyBinding::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1).handle.isValid());
    return 1;
}

int BodyBinding::getWorld(lua_State* L) {
    (void)lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1);
    lua_getiuservalue(L, 1, 1);
    return 1;
}

int BodyBinding::getType(lua_State* L) {
    lua::Stack::push(L, check(L).getType());
    return 1;
}

int BodyBinding::setType(lua_State* L) {
    check(L).setType(lua::Stack::read<haylen::physics2d::Body::Type>(L, 3));
    return 0;
}

int BodyBinding::getX(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().x);
    return 1;
}

int BodyBinding::setX(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    body.setTransform({lua::Stack::read<float>(L, 3), body.getPosition().y}, body.getRotation());
    return 0;
}

int BodyBinding::getY(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition().y);
    return 1;
}

int BodyBinding::setY(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    body.setTransform({body.getPosition().x, lua::Stack::read<float>(L, 3)}, body.getRotation());
    return 0;
}

int BodyBinding::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L).getPosition());
    return 1;
}

int BodyBinding::setPosition(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    body.setTransform(lua::Stack::read<haylen::math::Vec2>(L, 3), body.getRotation());
    return 0;
}

int BodyBinding::getRotation(lua_State* L) {
    lua::Stack::push(L, check(L).getRotation());
    return 1;
}

int BodyBinding::setRotation(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    body.setTransform(body.getPosition(), lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getVelocity(lua_State* L) {
    lua::Stack::push(L, check(L).getVelocity());
    return 1;
}

int BodyBinding::setVelocity(lua_State* L) {
    check(L).setVelocity(lua::Stack::read<haylen::math::Vec2>(L, 3));
    return 0;
}

int BodyBinding::getAngularVelocity(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularVelocity());
    return 1;
}

int BodyBinding::setAngularVelocity(lua_State* L) {
    check(L).setAngularVelocity(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getMass(lua_State* L) {
    lua::Stack::push(L, check(L).getMass());
    return 1;
}

int BodyBinding::setMass(lua_State* L) {
    check(L).setMass(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getCenterOfMass(lua_State* L) {
    lua::Stack::push(L, check(L).getCenterOfMass());
    return 1;
}

int BodyBinding::setCenterOfMass(lua_State* L) {
    check(L).setCenterOfMass(lua::Stack::read<haylen::math::Vec2>(L, 3));
    return 0;
}

int BodyBinding::getInertia(lua_State* L) {
    lua::Stack::push(L, check(L).getInertia());
    return 1;
}

int BodyBinding::setInertia(lua_State* L) {
    check(L).setInertia(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getWorldCenter(lua_State* L) {
    lua::Stack::push(L, check(L).getWorldCenter());
    return 1;
}

int BodyBinding::isSleepEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isSleepEnabled());
    return 1;
}

int BodyBinding::setSleepEnabled(lua_State* L) {
    check(L).setSleepEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyBinding::getSleepThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getSleepThreshold());
    return 1;
}

int BodyBinding::setSleepThreshold(lua_State* L) {
    check(L).setSleepThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::isAwake(lua_State* L) {
    lua::Stack::push(L, check(L).isAwake());
    return 1;
}

int BodyBinding::setAwake(lua_State* L) {
    check(L).setAwake(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyBinding::isEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isEnabled());
    return 1;
}

int BodyBinding::setEnabled(lua_State* L) {
    check(L).setEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyBinding::getLinearDamping(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearDamping());
    return 1;
}

int BodyBinding::setLinearDamping(lua_State* L) {
    check(L).setLinearDamping(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getAngularDamping(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularDamping());
    return 1;
}

int BodyBinding::setAngularDamping(lua_State* L) {
    check(L).setAngularDamping(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getGravityScale(lua_State* L) {
    lua::Stack::push(L, check(L).getGravityScale());
    return 1;
}

int BodyBinding::setGravityScale(lua_State* L) {
    check(L).setGravityScale(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::isLockedX(lua_State* L) {
    lua::Stack::push(L, check(L).getMotionLocks().x);
    return 1;
}

int BodyBinding::setLockedX(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    haylen::physics2d::Body::MotionLocks locks = body.getMotionLocks();
    locks.x = lua::Stack::read<bool>(L, 3);
    body.setMotionLocks(locks);
    return 0;
}

int BodyBinding::isLockedY(lua_State* L) {
    lua::Stack::push(L, check(L).getMotionLocks().y);
    return 1;
}

int BodyBinding::setLockedY(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    haylen::physics2d::Body::MotionLocks locks = body.getMotionLocks();
    locks.y = lua::Stack::read<bool>(L, 3);
    body.setMotionLocks(locks);
    return 0;
}

int BodyBinding::isRotationLocked(lua_State* L) {
    lua::Stack::push(L, check(L).getMotionLocks().rotation);
    return 1;
}

int BodyBinding::setRotationLocked(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    haylen::physics2d::Body::MotionLocks locks = body.getMotionLocks();
    locks.rotation = lua::Stack::read<bool>(L, 3);
    body.setMotionLocks(locks);
    return 0;
}

int BodyBinding::isBullet(lua_State* L) {
    lua::Stack::push(L, check(L).isBullet());
    return 1;
}

int BodyBinding::setBullet(lua_State* L) {
    check(L).setBullet(lua::Stack::read<bool>(L, 3));
    return 0;
}

int BodyBinding::getContinuousThreshold(lua_State* L) {
    lua::Stack::push(L, check(L).getContinuousThreshold());
    return 1;
}

int BodyBinding::setContinuousThreshold(lua_State* L) {
    check(L).setContinuousThreshold(lua::Stack::read<float>(L, 3));
    return 0;
}

int BodyBinding::getData(lua_State* L) {
    const std::uint64_t id = check(L).getId();
    pushDataSlot(L, id);
    lua_gettable(L, -2);
    return 1;
}

int BodyBinding::setData(lua_State* L) {
    const std::uint64_t id = check(L).getId();
    pushDataSlot(L, id);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    return 0;
}

int BodyBinding::applyForce(lua_State* L) {
    check(L).applyForce({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readOptionalPoint(L, 4));
    return 0;
}

int BodyBinding::applyImpulse(lua_State* L) {
    check(L).applyImpulse({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, readOptionalPoint(L, 4));
    return 0;
}

int BodyBinding::applyTorque(lua_State* L) {
    check(L).applyTorque(lua::Stack::read<float>(L, 2));
    return 0;
}

int BodyBinding::applyAngularImpulse(lua_State* L) {
    check(L).applyAngularImpulse(lua::Stack::read<float>(L, 2));
    return 0;
}

int BodyBinding::setTransform(lua_State* L) {
    check(L).setTransform({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, lua::Converter<float>::readOr(L, 4, 0.0));
    return 0;
}

// Moves a body by velocity to a transform with `moveTo(x, y[, rotation[, seconds]])`, where the rotation defaults to the current one and the time to one fixed step.
int BodyBinding::moveTo(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    const haylen::math::Vec2 position{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const float rotation = lua_isnoneornil(L, 4) ? body.getRotation() : lua::Stack::read<float>(L, 4);
    const auto seconds = lua::Converter<float>::readOr(L, 5, lua::Runtime::getEngine(L).getClock().getFixedStep());
    body.moveTo(position, rotation, seconds);
    return 0;
}

int BodyBinding::velocityAt(lua_State* L) {
    lua::Stack::push(L, check(L).getVelocityAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int BodyBinding::resetMass(lua_State* L) {
    check(L).resetMassData();
    return 0;
}

int BodyBinding::dropThrough(lua_State* L) {
    check(L).dropThrough(lua::Converter<float>::readOr(L, 2, kDropSeconds));
    return 0;
}

// Lists the touching contacts as `{shape, otherShape, other, x, y, normalX, normalY, impulse}`, where `other` is the body of the other shape.
int BodyBinding::contacts(lua_State* L) {
    const std::vector<haylen::physics2d::Body::Contact> found = check(L).getContacts();
    lua_getiuservalue(L, 1, 1);
    const int world = lua_gettop(L);
    lua_createtable(L, static_cast<int>(found.size()), 0);
    for (std::size_t index = 0; index < found.size(); ++index) {
        const haylen::physics2d::Body::Contact& contact = found[index];
        lua_createtable(L, 0, 13);
        Physics2DBinding::push(L, world, contact.shape);
        lua_setfield(L, -2, "shape");
        Physics2DBinding::push(L, world, contact.other);
        lua_setfield(L, -2, "otherShape");
        Physics2DBinding::push(L, world, contact.other.getBody());
        lua_setfield(L, -2, "other");
        Physics2DBinding::setManifold(L, contact.manifold);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Returns the `x`, `y` and `rotation` to draw the body at in this frame: blended between the last two steps by the interpolation of the frame clock when its world interpolates, and where the body is otherwise.
int BodyBinding::renderTransform(lua_State* L) {
    const haylen::physics2d::Body& body = check(L);
    haylen::physics2d::World::Transform transform{body.getPosition(), body.getRotation()};
    if (body.getWorld()->isInterpolating()) {
        transform = body.getWorld()->getInterpolatedTransform(body, static_cast<float>(lua::Runtime::getEngine(L).getClock().getInterpolation()));
    }

    lua::Stack::push(L, transform.position.x);
    lua::Stack::push(L, transform.position.y);
    lua::Stack::push(L, transform.rotation);
    return 3;
}

int BodyBinding::addBox(lua_State* L) {
    return pushNewShape(L, check(L).addBox({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, ShapeBinding::readOptions(L, 4)));
}

int BodyBinding::addCircle(lua_State* L) {
    return pushNewShape(L, check(L).addCircle(lua::Stack::read<float>(L, 2), ShapeBinding::readOptions(L, 3)));
}

int BodyBinding::addCapsule(lua_State* L) {
    return pushNewShape(L, check(L).addCapsule({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}, lua::Stack::read<float>(L, 6), ShapeBinding::readOptions(L, 7)));
}

int BodyBinding::addSegment(lua_State* L) {
    return pushNewShape(L, check(L).addSegment({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, {lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5)}, ShapeBinding::readOptions(L, 6)));
}

int BodyBinding::addPolygon(lua_State* L) {
    return pushNewShapes(L, check(L).addPolygon(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2), ShapeBinding::readOptions(L, 3)));
}

int BodyBinding::addChain(lua_State* L) {
    return pushNewShapes(L, check(L).addChain(lua::Stack::read<std::vector<haylen::math::Vec2>>(L, 2), lua_toboolean(L, 3) != 0, ShapeBinding::readOptions(L, 4)));
}

int BodyBinding::shapes(lua_State* L) {
    return pushNewShapes(L, check(L).getShapes());
}

int BodyBinding::outlines(lua_State* L) {
    const std::vector<haylen::physics2d::Shape::Outline> found = check(L).getOutlines();
    lua_createtable(L, static_cast<int>(found.size()), 0);
    for (std::size_t index = 0; index < found.size(); ++index) {
        ShapeBinding::pushOutline(L, found[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int BodyBinding::destroy(lua_State* L) {
    ScriptedHandle<haylen::physics2d::Body>& self = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1);

    if (!self.handle.isValid()) {
        return 0;
    }

    pushDataSlot(L, self.handle.getId());
    lua_pushnil(L);
    lua_settable(L, -3);
    self.handle.destroy();
    return 0;
}

// Moves the body to the world at index 2 and returns the body there, carrying its `data` from the table of this world to the table of that one.
int BodyBinding::moveToWorld(lua_State* L) {
    haylen::physics2d::Body& body = check(L);
    haylen::physics2d::World& target = lua::Userdata::check<haylen::physics2d::World>(L, 2);
    const std::uint64_t from = body.getId();
    const haylen::physics2d::Body moved = body.moveToWorld(target);

    pushDataSlot(L, from);
    lua_pushvalue(L, -1);
    lua_gettable(L, -3);
    const int value = lua_gettop(L);
    lua_pushvalue(L, -2);
    lua_pushnil(L);
    lua_settable(L, -5);

    lua_getiuservalue(L, 2, 1);
    lua_getfield(L, -1, "data");
    lua_pushinteger(L, static_cast<lua_Integer>(moved.getId()));
    lua_pushvalue(L, value);
    lua_settable(L, -3);

    Physics2DBinding::push(L, 2, moved);
    return 1;
}

int BodyBinding::equal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 1).handle == lua::Userdata::check<ScriptedHandle<haylen::physics2d::Body>>(L, 2).handle);
    return 1;
}

void BodyBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<haylen::physics2d::Body>>(L).function("addBox", &lua::Binding::native<&addBox>).function("addCircle", &lua::Binding::native<&addCircle>).function("addCapsule", &lua::Binding::native<&addCapsule>).function("addSegment", &lua::Binding::native<&addSegment>).function("addPolygon", &lua::Binding::native<&addPolygon>).function("addChain", &lua::Binding::native<&addChain>).function("shapes", &lua::Binding::native<&shapes>).function("outlines", &lua::Binding::native<&outlines>).function("applyForce", &lua::Binding::native<&applyForce>).function("applyImpulse", &lua::Binding::native<&applyImpulse>).function("applyTorque", &lua::Binding::native<&applyTorque>).function("applyAngularImpulse", &lua::Binding::native<&applyAngularImpulse>).function("setTransform", &lua::Binding::native<&setTransform>).function("moveTo", &lua::Binding::native<&moveTo>).function("velocityAt", &lua::Binding::native<&velocityAt>).function("resetMass", &lua::Binding::native<&resetMass>).function("dropThrough", &lua::Binding::native<&dropThrough>).function("contacts", &lua::Binding::native<&contacts>).function("renderTransform", &lua::Binding::native<&renderTransform>).function("destroy", &lua::Binding::native<&destroy>).function("moveToWorld", &lua::Binding::native<&moveToWorld>).property("valid", &isValid).property("world", &getWorld).property("type", &lua::Binding::native<&getType>, &lua::Binding::native<&setType>).property("x", &lua::Binding::native<&getX>, &lua::Binding::native<&setX>).property("y", &lua::Binding::native<&getY>, &lua::Binding::native<&setY>).property("position", &lua::Binding::native<&getPosition>, &lua::Binding::native<&setPosition>).property("rotation", &lua::Binding::native<&getRotation>, &lua::Binding::native<&setRotation>).property("velocity", &lua::Binding::native<&getVelocity>, &lua::Binding::native<&setVelocity>).property("angularVelocity", &lua::Binding::native<&getAngularVelocity>, &lua::Binding::native<&setAngularVelocity>).property("mass", &lua::Binding::native<&getMass>, &lua::Binding::native<&setMass>).property("centerOfMass", &lua::Binding::native<&getCenterOfMass>, &lua::Binding::native<&setCenterOfMass>).property("inertia", &lua::Binding::native<&getInertia>, &lua::Binding::native<&setInertia>).property("worldCenter", &lua::Binding::native<&getWorldCenter>).property("sleepEnabled", &lua::Binding::native<&isSleepEnabled>, &lua::Binding::native<&setSleepEnabled>).property("sleepThreshold", &lua::Binding::native<&getSleepThreshold>, &lua::Binding::native<&setSleepThreshold>).property("awake", &lua::Binding::native<&isAwake>, &lua::Binding::native<&setAwake>).property("enabled", &lua::Binding::native<&isEnabled>, &lua::Binding::native<&setEnabled>).property("linearDamping", &lua::Binding::native<&getLinearDamping>, &lua::Binding::native<&setLinearDamping>).property("angularDamping", &lua::Binding::native<&getAngularDamping>, &lua::Binding::native<&setAngularDamping>).property("gravityScale", &lua::Binding::native<&getGravityScale>, &lua::Binding::native<&setGravityScale>).property("lockX", &lua::Binding::native<&isLockedX>, &lua::Binding::native<&setLockedX>).property("lockY", &lua::Binding::native<&isLockedY>, &lua::Binding::native<&setLockedY>).property("lockRotation", &lua::Binding::native<&isRotationLocked>, &lua::Binding::native<&setRotationLocked>).property("bullet", &lua::Binding::native<&isBullet>, &lua::Binding::native<&setBullet>).property("continuousThreshold", &lua::Binding::native<&getContinuousThreshold>, &lua::Binding::native<&setContinuousThreshold>).property("data", &lua::Binding::native<&getData>, &lua::Binding::native<&setData>).meta("__eq", &equal).install();
}

} // namespace haylen::lua::physics2d
