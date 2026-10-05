#include "2d/physics/JointLua.hpp"

#include <optional>

#include "2d/physics/Physics2DLua.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::physics2d {

Joint& JointLua::check(lua_State* L) {
    ScriptedHandle<Joint>& self = lua::Userdata::check<ScriptedHandle<Joint>>(L, 1);
    if (!self.handle.isValid()) {
        luaL_error(L, "The physics joint was destroyed.");
    }
    return self.handle;
}

int JointLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.isValid());
    return 1;
}

int JointLua::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle.destroy();
    return 0;
}

int JointLua::getBodyA(lua_State* L) {
    const Body body = check(L).getBodyA();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int JointLua::getBodyB(lua_State* L) {
    const Body body = check(L).getBodyB();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int JointLua::getAnchorA(lua_State* L) {
    lua::Stack::push(L, check(L).getAnchorA());
    return 1;
}

int JointLua::getAnchorB(lua_State* L) {
    lua::Stack::push(L, check(L).getAnchorB());
    return 1;
}

int JointLua::equal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<Joint>>(L, 1).handle == lua::Userdata::check<ScriptedHandle<Joint>>(L, 2).handle);
    return 1;
}

int JointLua::getType(lua_State* L) {
    lua::Stack::push(L, check(L).getType());
    return 1;
}

int JointLua::getConstraintForce(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintForce());
    return 1;
}

int JointLua::getConstraintTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintTorque());
    return 1;
}

int JointLua::getLinearSeparation(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearSeparation());
    return 1;
}

int JointLua::getAngularSeparation(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularSeparation());
    return 1;
}

int JointLua::getConstraintHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintHertz());
    return 1;
}

int JointLua::setConstraintHertz(lua_State* L) {
    check(L).setConstraintHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getConstraintDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintDampingRatio());
    return 1;
}

int JointLua::setConstraintDampingRatio(lua_State* L) {
    check(L).setConstraintDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getBreakForce(lua_State* L) {
    lua::Stack::push(L, check(L).getBreakForce());
    return 1;
}

int JointLua::setBreakForce(lua_State* L) {
    check(L).setBreakForce(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointLua::getBreakTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getBreakTorque());
    return 1;
}

int JointLua::setBreakTorque(lua_State* L) {
    check(L).setBreakTorque(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointLua::getTarget(lua_State* L) {
    lua::Stack::push(L, check(L).getTarget());
    return 1;
}

int JointLua::setTarget(lua_State* L) {
    check(L).setTarget(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int JointLua::getAngle(lua_State* L) {
    lua::Stack::push(L, check(L).getAngle());
    return 1;
}

int JointLua::getTranslation(lua_State* L) {
    lua::Stack::push(L, check(L).getTranslation());
    return 1;
}

int JointLua::getCurrentLength(lua_State* L) {
    lua::Stack::push(L, check(L).getCurrentLength());
    return 1;
}

int JointLua::isLimitEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isLimitEnabled());
    return 1;
}

int JointLua::setLimitEnabled(lua_State* L) {
    check(L).setLimitEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointLua::getLower(lua_State* L) {
    lua::Stack::push(L, check(L).getLower());
    return 1;
}

int JointLua::setLower(lua_State* L) {
    Joint& joint = check(L);
    joint.setLimits(lua::Stack::read<float>(L, 3), joint.getUpper());
    return 0;
}

int JointLua::getUpper(lua_State* L) {
    lua::Stack::push(L, check(L).getUpper());
    return 1;
}

int JointLua::setUpper(lua_State* L) {
    Joint& joint = check(L);
    joint.setLimits(joint.getLower(), lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::isMotorEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isMotorEnabled());
    return 1;
}

int JointLua::setMotorEnabled(lua_State* L) {
    check(L).setMotorEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointLua::getMotorSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorSpeed());
    return 1;
}

int JointLua::setMotorSpeed(lua_State* L) {
    check(L).setMotorSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getMaxMotorForce(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxMotorForce());
    return 1;
}

int JointLua::setMaxMotorForce(lua_State* L) {
    check(L).setMaxMotorForce(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getMaxMotorTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxMotorTorque());
    return 1;
}

int JointLua::setMaxMotorTorque(lua_State* L) {
    check(L).setMaxMotorTorque(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getMotorForce(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorForce());
    return 1;
}

int JointLua::getMotorTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorTorque());
    return 1;
}

int JointLua::isSpringEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isSpringEnabled());
    return 1;
}

int JointLua::setSpringEnabled(lua_State* L) {
    check(L).setSpringEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointLua::getHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getHertz());
    return 1;
}

int JointLua::setHertz(lua_State* L) {
    check(L).setHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getDampingRatio());
    return 1;
}

int JointLua::setDampingRatio(lua_State* L) {
    check(L).setDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getLength(lua_State* L) {
    lua::Stack::push(L, check(L).getLength());
    return 1;
}

int JointLua::setLength(lua_State* L) {
    check(L).setLength(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getTargetAngle(lua_State* L) {
    lua::Stack::push(L, check(L).getTargetAngle());
    return 1;
}

int JointLua::setTargetAngle(lua_State* L) {
    check(L).setTargetAngle(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getTargetTranslation(lua_State* L) {
    lua::Stack::push(L, check(L).getTargetTranslation());
    return 1;
}

int JointLua::setTargetTranslation(lua_State* L) {
    check(L).setTargetTranslation(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointLua::getLinearOffset(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearOffset());
    return 1;
}

int JointLua::setLinearOffset(lua_State* L) {
    check(L).setLinearOffset(lua::Stack::read<math::Vec2>(L, 3));
    return 0;
}

int JointLua::getAngularOffset(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularOffset());
    return 1;
}

int JointLua::setAngularOffset(lua_State* L) {
    check(L).setAngularOffset(lua::Stack::read<float>(L, 3));
    return 0;
}

void JointLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<Joint>>(L).function("destroy", &lua::Binding::native<&destroy>).property("valid", &isValid).property("bodyA", &lua::Binding::native<&getBodyA>).property("bodyB", &lua::Binding::native<&getBodyB>).property("anchorA", &lua::Binding::native<&getAnchorA>).property("anchorB", &lua::Binding::native<&getAnchorB>).property("type", &lua::Binding::native<&getType>).property("constraintForce", &lua::Binding::native<&getConstraintForce>).property("constraintTorque", &lua::Binding::native<&getConstraintTorque>).property("linearSeparation", &lua::Binding::native<&getLinearSeparation>).property("angularSeparation", &lua::Binding::native<&getAngularSeparation>).property("constraintHertz", &lua::Binding::native<&getConstraintHertz>, &lua::Binding::native<&setConstraintHertz>).property("constraintDampingRatio", &lua::Binding::native<&getConstraintDampingRatio>, &lua::Binding::native<&setConstraintDampingRatio>).property("breakForce", &lua::Binding::native<&getBreakForce>, &lua::Binding::native<&setBreakForce>).property("breakTorque", &lua::Binding::native<&getBreakTorque>, &lua::Binding::native<&setBreakTorque>).property("target", &lua::Binding::native<&getTarget>, &lua::Binding::native<&setTarget>).property("angle", &lua::Binding::native<&getAngle>).property("translation", &lua::Binding::native<&getTranslation>).property("currentLength", &lua::Binding::native<&getCurrentLength>).property("enableLimit", &lua::Binding::native<&isLimitEnabled>, &lua::Binding::native<&setLimitEnabled>).property("lower", &lua::Binding::native<&getLower>, &lua::Binding::native<&setLower>).property("upper", &lua::Binding::native<&getUpper>, &lua::Binding::native<&setUpper>).property("enableMotor", &lua::Binding::native<&isMotorEnabled>, &lua::Binding::native<&setMotorEnabled>).property("motorSpeed", &lua::Binding::native<&getMotorSpeed>, &lua::Binding::native<&setMotorSpeed>).property("maxMotorForce", &lua::Binding::native<&getMaxMotorForce>, &lua::Binding::native<&setMaxMotorForce>).property("maxMotorTorque", &lua::Binding::native<&getMaxMotorTorque>, &lua::Binding::native<&setMaxMotorTorque>).property("motorForce", &lua::Binding::native<&getMotorForce>).property("motorTorque", &lua::Binding::native<&getMotorTorque>).property("enableSpring", &lua::Binding::native<&isSpringEnabled>, &lua::Binding::native<&setSpringEnabled>).property("hertz", &lua::Binding::native<&getHertz>, &lua::Binding::native<&setHertz>).property("dampingRatio", &lua::Binding::native<&getDampingRatio>, &lua::Binding::native<&setDampingRatio>).property("length", &lua::Binding::native<&getLength>, &lua::Binding::native<&setLength>).property("targetAngle", &lua::Binding::native<&getTargetAngle>, &lua::Binding::native<&setTargetAngle>).property("targetTranslation", &lua::Binding::native<&getTargetTranslation>, &lua::Binding::native<&setTargetTranslation>).property("linearOffset", &lua::Binding::native<&getLinearOffset>, &lua::Binding::native<&setLinearOffset>).property("angularOffset", &lua::Binding::native<&getAngularOffset>, &lua::Binding::native<&setAngularOffset>).meta("__eq", &equal).install();
}

} // namespace haylen::physics2d
