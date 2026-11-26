#include "3d/physics/BodyBinding.hpp"

#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>

#include "3d/physics/Physics3DBinding.hpp"
#include "3d/physics/ShapeBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/Physics3DPlugin.hpp"

namespace haylen::lua::physics3d {

using haylen::physics3d::Body;

Body& BodyBinding::check(lua_State* L, int index) {
    ScriptedBody& scripted = lua::Userdata::check<ScriptedBody>(L, index);
    if (scripted.world->isDestroyed()) {
        luaL_error(L, "The 3D physics world of the body was destroyed.");
    }

    if (!scripted.body.isValid()) {
        luaL_error(L, "The 3D physics body was destroyed.");
    }

    return scripted.body;
}

Body::MotionLocks BodyBinding::readLocks(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kLockFields});
    Body::MotionLocks locks;
    lua::Table::readField(L, index, "x", locks.x);
    lua::Table::readField(L, index, "y", locks.y);
    lua::Table::readField(L, index, "z", locks.z);
    lua::Table::readField(L, index, "rotationX", locks.rotationX);
    lua::Table::readField(L, index, "rotationY", locks.rotationY);
    lua::Table::readField(L, index, "rotationZ", locks.rotationZ);
    return locks;
}

void BodyBinding::pushLocks(lua_State* L, const Body::MotionLocks& locks) {
    lua_createtable(L, 0, static_cast<int>(kLockFields.size()));
    lua::Stack::push(L, locks.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, locks.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, locks.z);
    lua_setfield(L, -2, "z");
    lua::Stack::push(L, locks.rotationX);
    lua_setfield(L, -2, "rotationX");
    lua::Stack::push(L, locks.rotationY);
    lua_setfield(L, -2, "rotationY");
    lua::Stack::push(L, locks.rotationZ);
    lua_setfield(L, -2, "rotationZ");
}

// The shape is the one option a body needs, and every other option starts from the defaults of `Body::Options`.
Body::Options BodyBinding::readOptions(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kOptionFields});
    Body::Options options;
    lua_getfield(L, index, "shape");
    if (lua_isnil(L, -1)) {
        luaL_error(L, "A 3D body needs the option \"shape\", such as \"physics3d.newBox({1, 1, 1})\".");
    }

    options.shape = ShapeBinding::check(L, -1);
    lua_pop(L, 1);
    lua::Table::readField(L, index, "type", options.type);
    lua::Table::readField(L, index, "position", options.position);
    lua::Table::readField(L, index, "rotation", options.rotation);
    lua::Table::readField(L, index, "linearVelocity", options.linearVelocity);
    lua::Table::readField(L, index, "angularVelocity", options.angularVelocity);
    lua::Table::readField(L, index, "motionQuality", options.motionQuality);
    if (lua_getfield(L, index, "locks") != LUA_TNIL) {
        options.locks = readLocks(L, lua_gettop(L));
    }

    lua_pop(L, 1);
    lua::Table::readField(L, index, "mass", options.mass);
    lua::Table::readField(L, index, "inertia", options.inertia);
    lua::Table::readField(L, index, "centerOfMass", options.centerOfMass);
    lua::Table::readField(L, index, "linearDamping", options.linearDamping);
    lua::Table::readField(L, index, "angularDamping", options.angularDamping);
    lua::Table::readField(L, index, "gravityScale", options.gravityScale);
    lua::Table::readField(L, index, "maxLinearSpeed", options.maxLinearSpeed);
    lua::Table::readField(L, index, "maxAngularSpeed", options.maxAngularSpeed);
    lua::Table::readField(L, index, "friction", options.friction);
    lua::Table::readField(L, index, "restitution", options.restitution);
    lua::Table::readField(L, index, "frictionCombine", options.frictionCombine);
    lua::Table::readField(L, index, "restitutionCombine", options.restitutionCombine);
    lua::Table::readField(L, index, "sleepEnabled", options.sleepEnabled);
    lua::Table::readField(L, index, "sensor", options.sensor);
    lua::Table::readField(L, index, "collideWithStatic", options.collideWithStatic);
    lua::Table::readField(L, index, "enhancedInternalEdgeRemoval", options.enhancedInternalEdgeRemoval);
    lua::Table::readField(L, index, "enabled", options.enabled);
    if (lua_getfield(L, index, "category") != LUA_TNIL) {
        options.filter.category = readCategories(L, -1, "category");
    }

    lua_pop(L, 1);
    if (lua_getfield(L, index, "mask") != LUA_TNIL) {
        options.filter.mask = readCategories(L, -1, "mask");
    }

    lua_pop(L, 1);
    lua::Table::readField(L, index, "group", options.filter.group);
    return options;
}

std::uint32_t BodyBinding::readBits(lua_State* L, int index, std::string_view option) {
    if (lua_type(L, index) == LUA_TSTRING) {
        return lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Physics3DPlugin>().getCategories().getBit(lua_tostring(L, index));
    }

    int whole = 0;
    const lua_Integer bits = lua_tointegerx(L, index, &whole);
    if (whole == 0 || bits < 0 || bits > static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::invalid_argument(std::format("The option \"{}\" of a 3D body takes the names of categories or whole numbers of 32 bits.", option));
    }

    return static_cast<std::uint32_t>(bits);
}

std::uint32_t BodyBinding::readCategories(lua_State* L, int index, std::string_view option) {
    const int value = lua_absindex(L, index);
    if (lua_type(L, value) != LUA_TTABLE) {
        return readBits(L, value, option);
    }

    std::uint32_t bits = 0;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, value));
    for (lua_Integer item = 1; item <= count; ++item) {
        lua_rawgeti(L, value, item);
        bits |= readBits(L, -1, option);
        lua_pop(L, 1);
    }

    return bits;
}

void BodyBinding::pushDataSlot(lua_State* L, std::uint32_t id) {
    lua_getiuservalue(L, 1, 1);
    lua_getiuservalue(L, -1, 1);
    lua_getfield(L, -1, "data");
    lua_replace(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
}

template <int Axis> int BodyBinding::getAxis(lua_State* L) {
    const haylen::math::Vec3 position = check(L, 1).getPosition();
    lua::Stack::push(L, Axis == 0 ? position.x : (Axis == 1 ? position.y : position.z));
    return 1;
}

template <int Axis> int BodyBinding::setAxis(lua_State* L) {
    Body& body = check(L, 1);
    haylen::math::Vec3 position = body.getPosition();
    const float value = lua::Stack::read<float>(L, 3);
    if constexpr (Axis == 0) {
        position.x = value;
    } else if constexpr (Axis == 1) {
        position.y = value;
    } else {
        position.z = value;
    }

    body.setPosition(position);
    return 0;
}

int BodyBinding::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedBody>(L, 1).body.isValid());
    return 1;
}

int BodyBinding::getWorld(lua_State* L) {
    static_cast<void>(lua::Userdata::check<ScriptedBody>(L, 1));
    lua_getiuservalue(L, 1, 1);
    return 1;
}

int BodyBinding::getLocks(lua_State* L) {
    pushLocks(L, check(L, 1).getMotionLocks());
    return 1;
}

int BodyBinding::setLocks(lua_State* L) {
    Body& body = check(L, 1);
    body.setMotionLocks(readLocks(L, 3));
    return 0;
}

int BodyBinding::getCategory(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L, 1).getFilter().category));
    return 1;
}

int BodyBinding::setCategory(lua_State* L) {
    Body& body = check(L, 1);
    haylen::physics3d::CollisionFilter filter = body.getFilter();
    filter.category = readCategories(L, 3, "category");
    body.setFilter(filter);
    return 0;
}

int BodyBinding::getMask(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L, 1).getFilter().mask));
    return 1;
}

int BodyBinding::setMask(lua_State* L) {
    Body& body = check(L, 1);
    haylen::physics3d::CollisionFilter filter = body.getFilter();
    filter.mask = readCategories(L, 3, "mask");
    body.setFilter(filter);
    return 0;
}

int BodyBinding::getGroup(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getFilter().group);
    return 1;
}

int BodyBinding::setGroup(lua_State* L) {
    Body& body = check(L, 1);
    haylen::physics3d::CollisionFilter filter = body.getFilter();
    filter.group = lua::Stack::read<int>(L, 3);
    body.setFilter(filter);
    return 0;
}

int BodyBinding::getShape(lua_State* L) {
    ShapeBinding::push(L, check(L, 1).getShape());
    return 1;
}

int BodyBinding::setShape(lua_State* L) {
    Body& body = check(L, 1);
    body.setShape(ShapeBinding::check(L, 3));
    return 0;
}

int BodyBinding::getData(lua_State* L) {
    const std::uint32_t id = check(L, 1).getId();
    pushDataSlot(L, id);
    lua_gettable(L, -2);
    return 1;
}

int BodyBinding::setData(lua_State* L) {
    const std::uint32_t id = check(L, 1).getId();
    pushDataSlot(L, id);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    return 0;
}

int BodyBinding::getChildCount(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getChildCount());
    return 1;
}

// Teleports the body with `setTransform(position[, rotation])`, keeping its rotation when the call names none.
int BodyBinding::setTransform(lua_State* L) {
    Body& body = check(L, 1);
    const haylen::math::Vec3 position = lua::Stack::read<haylen::math::Vec3>(L, 2);
    const haylen::math::Quat rotation = lua_isnoneornil(L, 3) ? body.getRotation() : lua::Stack::read<haylen::math::Quat>(L, 3);
    body.setTransform({.position = position, .rotation = rotation});
    return 0;
}

// Moves the body to the transform over `seconds` with `moveTo(position, rotation, seconds)`, which carries what it touches.
int BodyBinding::moveTo(lua_State* L) {
    Body& body = check(L, 1);
    body.moveTo({.position = lua::Stack::read<haylen::math::Vec3>(L, 2), .rotation = lua::Stack::read<haylen::math::Quat>(L, 3)}, lua::Stack::read<float>(L, 4));
    return 0;
}

// Pushes the body with `applyForce(force[, point])`, at the world point when the call names one.
int BodyBinding::applyForce(lua_State* L) {
    Body& body = check(L, 1);
    body.applyForce(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<std::optional<haylen::math::Vec3>>(L, 3));
    return 0;
}

int BodyBinding::applyImpulse(lua_State* L) {
    Body& body = check(L, 1);
    body.applyImpulse(lua::Stack::read<haylen::math::Vec3>(L, 2), lua::Stack::read<std::optional<haylen::math::Vec3>>(L, 3));
    return 0;
}

int BodyBinding::applyTorque(lua_State* L) {
    Body& body = check(L, 1);
    body.applyTorque(lua::Stack::read<haylen::math::Vec3>(L, 2));
    return 0;
}

int BodyBinding::applyAngularImpulse(lua_State* L) {
    Body& body = check(L, 1);
    body.applyAngularImpulse(lua::Stack::read<haylen::math::Vec3>(L, 2));
    return 0;
}

int BodyBinding::velocityAt(lua_State* L) {
    Body& body = check(L, 1);
    lua::Stack::push(L, body.getVelocityAt(lua::Stack::read<haylen::math::Vec3>(L, 2)));
    return 1;
}

int BodyBinding::resetMass(lua_State* L) {
    check(L, 1).resetMass();
    return 0;
}

// Adds a child to the own mutable compound of the body with `addChild(shape[, position[, rotation]])` and returns its index.
int BodyBinding::addChild(lua_State* L) {
    Body& body = check(L, 1);
    haylen::physics3d::Shape::Child child{.shape = ShapeBinding::check(L, 2)};
    if (!lua_isnoneornil(L, 3)) {
        child.position = lua::Stack::read<haylen::math::Vec3>(L, 3);
    }

    if (!lua_isnoneornil(L, 4)) {
        child.rotation = lua::Stack::read<haylen::math::Quat>(L, 4);
    }

    lua::Stack::push(L, body.addChild(child) + 1);
    return 1;
}

std::size_t BodyBinding::checkChild(lua_State* L, const Body& body, int index) {
    const lua_Integer child = luaL_checkinteger(L, index);
    const std::size_t count = body.getChildCount();
    if (child < 1 || static_cast<std::size_t>(child) > count) {
        luaL_error(L, "The 3D body has no child %d, since its children go from 1 to %d.", static_cast<int>(child), static_cast<int>(count));
    }

    return static_cast<std::size_t>(child - 1);
}

int BodyBinding::removeChild(lua_State* L) {
    Body& body = check(L, 1);
    const std::size_t index = checkChild(L, body, 2);
    body.removeChild(index);
    return 0;
}

int BodyBinding::setChildTransform(lua_State* L) {
    Body& body = check(L, 1);
    const std::size_t index = checkChild(L, body, 2);
    body.setChildTransform(index, {.position = lua::Stack::read<haylen::math::Vec3>(L, 3), .rotation = lua::Stack::read<haylen::math::Quat>(L, 4)});
    return 0;
}

// Returns the position and rotation to draw the body at with `renderTransform()`, between its last two steps by the interpolation of the frame clock when its world interpolates.
int BodyBinding::renderTransform(lua_State* L) {
    const Body& body = check(L, 1);
    const haylen::physics3d::World& world = *lua::Userdata::check<ScriptedBody>(L, 1).world;
    Body::Transform transform = body.getTransform();
    if (world.isInterpolating()) {
        transform = world.getInterpolatedTransform(body, static_cast<float>(lua::Runtime::getEngine(L).getClock().getInterpolation()));
    }

    lua::Stack::push(L, transform.position);
    lua::Stack::push(L, transform.rotation);
    return 2;
}

int BodyBinding::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedBody>(L, 1).body.destroy();
    return 0;
}

int BodyBinding::equal(lua_State* L) {
    const ScriptedBody* lhs = lua::Userdata::test<ScriptedBody>(L, 1);
    const ScriptedBody* rhs = lua::Userdata::test<ScriptedBody>(L, 2);
    lua_pushboolean(L, lhs != nullptr && rhs != nullptr && lhs->body == rhs->body ? 1 : 0);
    return 1;
}

void BodyBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedBody>(L).function("setTransform", &lua::Binding::native<&setTransform>).function("moveTo", &lua::Binding::native<&moveTo>).function("applyForce", &lua::Binding::native<&applyForce>).function("applyImpulse", &lua::Binding::native<&applyImpulse>).function("applyTorque", &lua::Binding::native<&applyTorque>).function("applyAngularImpulse", &lua::Binding::native<&applyAngularImpulse>).function("velocityAt", &lua::Binding::native<&velocityAt>).function("resetMass", &lua::Binding::native<&resetMass>).function("addChild", &lua::Binding::native<&addChild>).function("removeChild", &lua::Binding::native<&removeChild>).function("setChildTransform", &lua::Binding::native<&setChildTransform>).function("renderTransform", &lua::Binding::native<&renderTransform>).function("destroy", &lua::Binding::native<&destroy>).property("valid", &isValid).property("world", &getWorld).liveAccessor<&ScriptedBody::getPosition, &ScriptedBody::setPosition>("position").liveAccessor<&ScriptedBody::getRotation, &ScriptedBody::setRotation>("rotation").liveAccessor<&ScriptedBody::getLinearVelocity, &ScriptedBody::setLinearVelocity>("linearVelocity").liveAccessor<&ScriptedBody::getAngularVelocity, &ScriptedBody::setAngularVelocity>("angularVelocity").property("x", &lua::Binding::native<&getAxis<0>>, &lua::Binding::native<&setAxis<0>>).property("y", &lua::Binding::native<&getAxis<1>>, &lua::Binding::native<&setAxis<1>>).property("z", &lua::Binding::native<&getAxis<2>>, &lua::Binding::native<&setAxis<2>>).property("type", &lua::Binding::native<&get<&Body::getType>>, &lua::Binding::native<&set<&Body::setType, Body::Type>>).property("motionQuality", &lua::Binding::native<&get<&Body::getMotionQuality>>, &lua::Binding::native<&set<&Body::setMotionQuality, Body::MotionQuality>>).property("locks", &lua::Binding::native<&getLocks>, &lua::Binding::native<&setLocks>).property("mass", &lua::Binding::native<&get<&Body::getMass>>, &lua::Binding::native<&set<&Body::setMass, float>>).property("inertia", &lua::Binding::native<&get<&Body::getInertia>>, &lua::Binding::native<&set<&Body::setInertia, haylen::math::Vec3>>).property("centerOfMass", &lua::Binding::native<&get<&Body::getCenterOfMass>>, &lua::Binding::native<&set<&Body::setCenterOfMass, haylen::math::Vec3>>).property("worldCenter", &lua::Binding::native<&get<&Body::getWorldCenter>>).property("linearDamping", &lua::Binding::native<&get<&Body::getLinearDamping>>, &lua::Binding::native<&set<&Body::setLinearDamping, float>>).property("angularDamping", &lua::Binding::native<&get<&Body::getAngularDamping>>, &lua::Binding::native<&set<&Body::setAngularDamping, float>>).property("gravityScale", &lua::Binding::native<&get<&Body::getGravityScale>>, &lua::Binding::native<&set<&Body::setGravityScale, float>>).property("maxLinearSpeed", &lua::Binding::native<&get<&Body::getMaxLinearSpeed>>, &lua::Binding::native<&set<&Body::setMaxLinearSpeed, float>>).property("maxAngularSpeed", &lua::Binding::native<&get<&Body::getMaxAngularSpeed>>, &lua::Binding::native<&set<&Body::setMaxAngularSpeed, float>>).property("friction", &lua::Binding::native<&get<&Body::getFriction>>, &lua::Binding::native<&set<&Body::setFriction, float>>).property("restitution", &lua::Binding::native<&get<&Body::getRestitution>>, &lua::Binding::native<&set<&Body::setRestitution, float>>).property("frictionCombine", &lua::Binding::native<&get<&Body::getFrictionCombine>>, &lua::Binding::native<&set<&Body::setFrictionCombine, Body::CombineRule>>).property("restitutionCombine", &lua::Binding::native<&get<&Body::getRestitutionCombine>>, &lua::Binding::native<&set<&Body::setRestitutionCombine, Body::CombineRule>>).property("sleepEnabled", &lua::Binding::native<&get<&Body::isSleepEnabled>>, &lua::Binding::native<&set<&Body::setSleepEnabled, bool>>).property("awake", &lua::Binding::native<&get<&Body::isAwake>>, &lua::Binding::native<&set<&Body::setAwake, bool>>).property("sensor", &lua::Binding::native<&get<&Body::isSensor>>, &lua::Binding::native<&set<&Body::setSensor, bool>>).property("collideWithStatic", &lua::Binding::native<&get<&Body::isCollidingWithStatic>>, &lua::Binding::native<&set<&Body::setCollideWithStatic, bool>>).property("enhancedInternalEdgeRemoval", &lua::Binding::native<&get<&Body::hasEnhancedInternalEdgeRemoval>>, &lua::Binding::native<&set<&Body::setEnhancedInternalEdgeRemoval, bool>>).property("enabled", &lua::Binding::native<&get<&Body::isEnabled>>, &lua::Binding::native<&set<&Body::setEnabled, bool>>).property("category", &lua::Binding::native<&getCategory>, &lua::Binding::native<&setCategory>).property("mask", &lua::Binding::native<&getMask>, &lua::Binding::native<&setMask>).property("group", &lua::Binding::native<&getGroup>, &lua::Binding::native<&setGroup>).property("shape", &lua::Binding::native<&getShape>, &lua::Binding::native<&setShape>).property("bounds", &lua::Binding::native<&get<&Body::getBounds>>).property("childCount", &lua::Binding::native<&getChildCount>).property("data", &lua::Binding::native<&getData>, &lua::Binding::native<&setData>).meta("__eq", &equal).install();
}

} // namespace haylen::lua::physics3d
