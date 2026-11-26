#include "3d/physics/JointBinding.hpp"

#include <cstddef>
#include <optional>
#include <string>

#include "3d/physics/Physics3DBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "math/Curve3DBinding.hpp"

namespace haylen::lua {

template <> struct Type<lua::physics3d::ScriptedJoint> {
    static constexpr const char* name = "haylen.Joint3D";
    using Storage = lua::physics3d::ScriptedJoint;
};

} // namespace haylen::lua

namespace haylen::lua::physics3d {

using haylen::physics3d::Joint;

void JointBinding::push(lua_State* L, int worldIndex, const Joint& joint) {
    const int owner = lua_absindex(L, worldIndex);
    lua::Userdata::emplace<ScriptedJoint>(L, ScriptedJoint{.world = lua::Userdata::checkShared<haylen::physics3d::World>(L, owner), .joint = joint});
    lua_pushvalue(L, owner);
    lua_setiuservalue(L, -2, 1);
}

Joint& JointBinding::check(lua_State* L, int index) {
    ScriptedJoint& scripted = lua::Userdata::check<ScriptedJoint>(L, index);
    if (scripted.world->isDestroyed()) {
        luaL_error(L, "The 3D physics world of the joint was destroyed.");
    }

    if (!scripted.joint.isValid()) {
        luaL_error(L, "The 3D physics joint was destroyed.");
    }

    return scripted.joint;
}

Joint::Spring JointBinding::readSpring(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kSpringFields});
    Joint::Spring spring;
    lua::Table::readField(L, table, "mode", spring.mode);
    lua::Table::readField(L, table, "frequency", spring.frequency);
    lua::Table::readField(L, table, "stiffness", spring.stiffness);
    lua::Table::readField(L, table, "damping", spring.damping);
    return spring;
}

Joint::Motor JointBinding::readMotor(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kMotorFields});
    Joint::Motor motor;
    lua::Table::readField(L, table, "state", motor.state);
    lua::Table::readField(L, table, "targetVelocity", motor.targetVelocity);
    lua::Table::readField(L, table, "targetPosition", motor.targetPosition);
    lua::Table::readField(L, table, "maxForce", motor.maxForce);
    lua::Table::readField(L, table, "maxTorque", motor.maxTorque);
    if (lua_getfield(L, table, "spring") != LUA_TNIL) {
        motor.spring = readSpring(L, -1);
    }

    lua_pop(L, 1);
    return motor;
}

Joint::AxisSettings JointBinding::readAxis(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kAxisFields});
    Joint::AxisSettings axis;
    lua::Table::readField(L, table, "mode", axis.mode);
    lua::Table::readField(L, table, "lower", axis.lower);
    lua::Table::readField(L, table, "upper", axis.upper);
    lua::Table::readField(L, table, "friction", axis.friction);
    if (lua_getfield(L, table, "limitSpring") != LUA_TNIL) {
        axis.limitSpring = readSpring(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "motor") != LUA_TNIL) {
        axis.motor = readMotor(L, -1);
    }

    lua_pop(L, 1);
    return axis;
}

void JointBinding::pushSpring(lua_State* L, const Joint::Spring& spring) {
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, spring.mode);
    lua_setfield(L, -2, "mode");
    lua::Stack::push(L, spring.frequency);
    lua_setfield(L, -2, "frequency");
    lua::Stack::push(L, spring.stiffness);
    lua_setfield(L, -2, "stiffness");
    lua::Stack::push(L, spring.damping);
    lua_setfield(L, -2, "damping");
}

void JointBinding::pushMotor(lua_State* L, const Joint::Motor& motor) {
    lua_createtable(L, 0, 6);
    lua::Stack::push(L, motor.state);
    lua_setfield(L, -2, "state");
    lua::Stack::push(L, motor.targetVelocity);
    lua_setfield(L, -2, "targetVelocity");
    lua::Stack::push(L, motor.targetPosition);
    lua_setfield(L, -2, "targetPosition");
    lua::Stack::push(L, motor.maxForce);
    lua_setfield(L, -2, "maxForce");
    lua::Stack::push(L, motor.maxTorque);
    lua_setfield(L, -2, "maxTorque");
    pushSpring(L, motor.spring);
    lua_setfield(L, -2, "spring");
}

void JointBinding::pushAxis(lua_State* L, const Joint::AxisSettings& axis) {
    lua_createtable(L, 0, 6);
    lua::Stack::push(L, axis.mode);
    lua_setfield(L, -2, "mode");
    lua::Stack::push(L, axis.lower);
    lua_setfield(L, -2, "lower");
    lua::Stack::push(L, axis.upper);
    lua_setfield(L, -2, "upper");
    pushSpring(L, axis.limitSpring);
    lua_setfield(L, -2, "limitSpring");
    lua::Stack::push(L, axis.friction);
    lua_setfield(L, -2, "friction");
    pushMotor(L, axis.motor);
    lua_setfield(L, -2, "motor");
}

Joint::MotorSlot JointBinding::slotOf(Joint::Axis axis) noexcept {
    return static_cast<Joint::MotorSlot>(static_cast<std::size_t>(Joint::MotorSlot::X) + static_cast<std::size_t>(axis));
}

// The axes are a table keyed by the names of the axes, `x`, `y`, `z`, `rotationX`, `rotationY` and `rotationZ`, and the joints of a gear or a rack and pinion a list of two joints.
Joint::Options JointBinding::readOptions(lua_State* L, int index) {
    Joint::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }

    const int table = lua_absindex(L, index);
    luaL_checktype(L, table, LUA_TTABLE);
    lua::Table::checkFields(L, table, {kOptionFields});
    lua::Table::readField(L, table, "point", options.point);
    lua::Table::readField(L, table, "secondPoint", options.secondPoint);
    lua::Table::readField(L, table, "axis", options.axis);
    lua::Table::readField(L, table, "normal", options.normal);
    lua::Table::readField(L, table, "secondAxis", options.secondAxis);
    lua::Table::readField(L, table, "lower", options.lower);
    lua::Table::readField(L, table, "upper", options.upper);
    lua::Table::readField(L, table, "friction", options.friction);
    lua::Table::readField(L, table, "halfConeAngle", options.halfConeAngle);
    lua::Table::readField(L, table, "planeHalfConeAngle", options.planeHalfConeAngle);
    lua::Table::readField(L, table, "targetRotation", options.targetRotation);
    lua::Table::readField(L, table, "targetAngularVelocity", options.targetAngularVelocity);
    lua::Table::readField(L, table, "pathRotation", options.pathRotation);
    lua::Table::readField(L, table, "pathFraction", options.pathFraction);
    lua::Table::readField(L, table, "ratio", options.ratio);
    lua::Table::readField(L, table, "fixedPoint", options.fixedPoint);
    lua::Table::readField(L, table, "secondFixedPoint", options.secondFixedPoint);
    lua::Table::readField(L, table, "priority", options.priority);
    lua::Table::readField(L, table, "velocitySteps", options.velocitySteps);
    lua::Table::readField(L, table, "positionSteps", options.positionSteps);
    lua::Table::readField(L, table, "collideConnected", options.collideConnected);
    lua::Table::readField(L, table, "enabled", options.enabled);
    lua::Table::readField(L, table, "breakForce", options.breakForce);
    lua::Table::readField(L, table, "breakTorque", options.breakTorque);
    if (lua_getfield(L, table, "limitSpring") != LUA_TNIL) {
        options.limitSpring = readSpring(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "motor") != LUA_TNIL) {
        options.motor = readMotor(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "swingMotor") != LUA_TNIL) {
        options.swingMotor = readMotor(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "path") != LUA_TNIL) {
        options.path = lua::Userdata::check<haylen::math::Spline3D>(L, -1);
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "joints") != LUA_TNIL) {
        lua::Table::requireType(L, "joints", LUA_TTABLE);
        for (std::size_t item = 0; item < options.joints.size(); ++item) {
            lua_rawgeti(L, -1, static_cast<lua_Integer>(item + 1));
            options.joints[item] = check(L, -1);
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    if (lua_getfield(L, table, "axes") != LUA_TNIL) {
        lua::Table::requireType(L, "axes", LUA_TTABLE);
        const int axes = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, axes) != 0) {
            const std::optional<Joint::Axis> axis = lua_type(L, -2) == LUA_TSTRING ? Joint::axisFromName(lua_tostring(L, -2)) : std::nullopt;
            if (!axis) {
                luaL_error(L, "The axes of a 3D joint are \"x\", \"y\", \"z\", \"rotationX\", \"rotationY\" and \"rotationZ\".");
            }

            options.axes[static_cast<std::size_t>(*axis)] = readAxis(L, -1);
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    return options;
}

int JointBinding::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedJoint>(L, 1).joint.isValid());
    return 1;
}

int JointBinding::getType(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getType());
    return 1;
}

int JointBinding::getFirst(lua_State* L) {
    const haylen::physics3d::Body first = check(L, 1).getFirst();
    lua_getiuservalue(L, 1, 1);
    Physics3DBinding::pushBody(L, -1, first);
    return 1;
}

int JointBinding::getSecond(lua_State* L) {
    const std::optional<haylen::physics3d::Body> second = check(L, 1).getSecond();
    if (!second) {
        lua_pushnil(L);
        return 1;
    }

    lua_getiuservalue(L, 1, 1);
    Physics3DBinding::pushBody(L, -1, *second);
    return 1;
}

int JointBinding::isEnabled(lua_State* L) {
    lua::Stack::push(L, check(L, 1).isEnabled());
    return 1;
}

int JointBinding::setEnabled(lua_State* L) {
    check(L, 1).setEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointBinding::getPriority(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getPriority());
    return 1;
}

int JointBinding::setPriority(lua_State* L) {
    check(L, 1).setPriority(lua::Stack::read<int>(L, 3));
    return 0;
}

int JointBinding::getForce(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getLoad().force);
    return 1;
}

int JointBinding::getTorque(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getLoad().torque);
    return 1;
}

int JointBinding::getBreakForce(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getBreakForce());
    return 1;
}

int JointBinding::setBreakForce(lua_State* L) {
    check(L, 1).setBreakForce(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointBinding::getBreakTorque(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getBreakTorque());
    return 1;
}

int JointBinding::setBreakTorque(lua_State* L) {
    check(L, 1).setBreakTorque(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointBinding::getLower(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getLower());
    return 1;
}

int JointBinding::getUpper(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getUpper());
    return 1;
}

// Changes both limits at once with `setLimits(lower, upper)`, so the lower one never passes the upper one between two writes.
int JointBinding::setLimits(lua_State* L) {
    check(L, 1).setLimits(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getLimitSpring(lua_State* L) {
    pushSpring(L, check(L, 1).getLimitSpring());
    return 1;
}

int JointBinding::setLimitSpring(lua_State* L) {
    Joint& joint = check(L, 1);
    joint.setLimitSpring(readSpring(L, 3));
    return 0;
}

int JointBinding::getFriction(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getFriction());
    return 1;
}

int JointBinding::setFriction(lua_State* L) {
    check(L, 1).setFriction(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getHalfConeAngle(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getHalfConeAngle());
    return 1;
}

int JointBinding::setHalfConeAngle(lua_State* L) {
    check(L, 1).setHalfConeAngle(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getPlaneHalfConeAngle(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getPlaneHalfConeAngle());
    return 1;
}

int JointBinding::setPlaneHalfConeAngle(lua_State* L) {
    check(L, 1).setPlaneHalfConeAngle(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getRatio(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getRatio());
    return 1;
}

int JointBinding::getAngle(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getAngle());
    return 1;
}

int JointBinding::getPosition(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getPosition());
    return 1;
}

int JointBinding::getLength(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getLength());
    return 1;
}

int JointBinding::getFraction(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getFraction());
    return 1;
}

int JointBinding::getMotor(lua_State* L) {
    pushMotor(L, check(L, 1).getMotor());
    return 1;
}

int JointBinding::setMotor(lua_State* L) {
    Joint& joint = check(L, 1);
    joint.setMotor(readMotor(L, 3));
    return 0;
}

int JointBinding::getSwingMotor(lua_State* L) {
    pushMotor(L, check(L, 1).getMotor(Joint::MotorSlot::Swing));
    return 1;
}

int JointBinding::setSwingMotor(lua_State* L) {
    Joint& joint = check(L, 1);
    joint.setMotor(readMotor(L, 3), Joint::MotorSlot::Swing);
    return 0;
}

int JointBinding::getTargetRotation(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getTargetRotation());
    return 1;
}

int JointBinding::setTargetRotation(lua_State* L) {
    check(L, 1).setTargetRotation(lua::Stack::read<haylen::math::Quat>(L, 3));
    return 0;
}

int JointBinding::getTargetAngularVelocity(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getTargetAngularVelocity());
    return 1;
}

int JointBinding::setTargetAngularVelocity(lua_State* L) {
    check(L, 1).setTargetAngularVelocity(lua::Stack::read<haylen::math::Vec3>(L, 3));
    return 0;
}

// Returns the settings of an axis of a six degrees of freedom joint with `axis(name)`.
int JointBinding::getAxis(lua_State* L) {
    const Joint& joint = check(L, 1);
    pushAxis(L, joint.getAxis(lua::Stack::read<Joint::Axis>(L, 2)));
    return 1;
}

// Changes an axis of a six degrees of freedom joint with `setAxis(name, {mode, lower, upper, limitSpring, friction, motor})`.
int JointBinding::setAxis(lua_State* L) {
    Joint& joint = check(L, 1);
    joint.setAxis(lua::Stack::read<Joint::Axis>(L, 2), readAxis(L, 3));
    return 0;
}

int JointBinding::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedJoint>(L, 1).joint.destroy();
    return 0;
}

int JointBinding::equal(lua_State* L) {
    const ScriptedJoint* lhs = lua::Userdata::test<ScriptedJoint>(L, 1);
    const ScriptedJoint* rhs = lua::Userdata::test<ScriptedJoint>(L, 2);
    lua_pushboolean(L, lhs != nullptr && rhs != nullptr && lhs->joint == rhs->joint ? 1 : 0);
    return 1;
}

void JointBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedJoint>(L).function("setLimits", &lua::Binding::native<&setLimits>).function("axis", &lua::Binding::native<&getAxis>).function("setAxis", &lua::Binding::native<&setAxis>).function("destroy", &lua::Binding::native<&destroy>).property("valid", &isValid).property("type", &lua::Binding::native<&getType>).property("first", &lua::Binding::native<&getFirst>).property("second", &lua::Binding::native<&getSecond>).property("enabled", &lua::Binding::native<&isEnabled>, &lua::Binding::native<&setEnabled>).property("priority", &lua::Binding::native<&getPriority>, &lua::Binding::native<&setPriority>).property("force", &lua::Binding::native<&getForce>).property("torque", &lua::Binding::native<&getTorque>).property("breakForce", &lua::Binding::native<&getBreakForce>, &lua::Binding::native<&setBreakForce>).property("breakTorque", &lua::Binding::native<&getBreakTorque>, &lua::Binding::native<&setBreakTorque>).property("lower", &lua::Binding::native<&getLower>).property("upper", &lua::Binding::native<&getUpper>).property("limitSpring", &lua::Binding::native<&getLimitSpring>, &lua::Binding::native<&setLimitSpring>).property("friction", &lua::Binding::native<&getFriction>, &lua::Binding::native<&setFriction>).property("halfConeAngle", &lua::Binding::native<&getHalfConeAngle>, &lua::Binding::native<&setHalfConeAngle>).property("planeHalfConeAngle", &lua::Binding::native<&getPlaneHalfConeAngle>, &lua::Binding::native<&setPlaneHalfConeAngle>).property("ratio", &lua::Binding::native<&getRatio>).property("angle", &lua::Binding::native<&getAngle>).property("position", &lua::Binding::native<&getPosition>).property("length", &lua::Binding::native<&getLength>).property("fraction", &lua::Binding::native<&getFraction>).property("motor", &lua::Binding::native<&getMotor>, &lua::Binding::native<&setMotor>).property("swingMotor", &lua::Binding::native<&getSwingMotor>, &lua::Binding::native<&setSwingMotor>).property("targetRotation", &lua::Binding::native<&getTargetRotation>, &lua::Binding::native<&setTargetRotation>).property("targetAngularVelocity", &lua::Binding::native<&getTargetAngularVelocity>, &lua::Binding::native<&setTargetAngularVelocity>).meta("__eq", &equal).install();
}

} // namespace haylen::lua::physics3d
