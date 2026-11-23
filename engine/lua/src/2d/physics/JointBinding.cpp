#include "2d/physics/JointBinding.hpp"

#include <optional>

#include "2d/physics/Physics2DBinding.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua::physics2d {

haylen::physics2d::Joint& JointBinding::check(lua_State* L) {
    ScriptedHandle<haylen::physics2d::Joint>& self = lua::Userdata::check<ScriptedHandle<haylen::physics2d::Joint>>(L, 1);

    if (self.world->isDestroyed()) {
        luaL_error(L, "The physics world of the joint was destroyed.");
    }

    if (!self.handle.isValid()) {
        luaL_error(L, "The physics joint was destroyed.");
    }

    return self.handle;
}

int JointBinding::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<haylen::physics2d::Joint>>(L, 1).handle.isValid());
    return 1;
}

int JointBinding::destroy(lua_State* L) {
    lua::Userdata::check<ScriptedHandle<haylen::physics2d::Joint>>(L, 1).handle.destroy();
    return 0;
}

int JointBinding::getBodyA(lua_State* L) {
    const haylen::physics2d::Body body = check(L).getBodyA();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int JointBinding::getBodyB(lua_State* L) {
    const haylen::physics2d::Body body = check(L).getBodyB();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int JointBinding::getAnchorA(lua_State* L) {
    lua::Stack::push(L, check(L).getAnchorA());
    return 1;
}

int JointBinding::getAnchorB(lua_State* L) {
    lua::Stack::push(L, check(L).getAnchorB());
    return 1;
}

int JointBinding::equal(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedHandle<haylen::physics2d::Joint>>(L, 1).handle == lua::Userdata::check<ScriptedHandle<haylen::physics2d::Joint>>(L, 2).handle);
    return 1;
}

int JointBinding::getType(lua_State* L) {
    lua::Stack::push(L, check(L).getType());
    return 1;
}

int JointBinding::getConstraintForce(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintForce());
    return 1;
}

int JointBinding::getConstraintTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintTorque());
    return 1;
}

int JointBinding::getLinearSeparation(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearSeparation());
    return 1;
}

int JointBinding::getAngularSeparation(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularSeparation());
    return 1;
}

int JointBinding::getConstraintHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintHertz());
    return 1;
}

int JointBinding::setConstraintHertz(lua_State* L) {
    check(L).setConstraintHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getConstraintDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getConstraintDampingRatio());
    return 1;
}

int JointBinding::setConstraintDampingRatio(lua_State* L) {
    check(L).setConstraintDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getBreakForce(lua_State* L) {
    lua::Stack::push(L, check(L).getBreakForce());
    return 1;
}

int JointBinding::setBreakForce(lua_State* L) {
    check(L).setBreakForce(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointBinding::getBreakTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getBreakTorque());
    return 1;
}

int JointBinding::setBreakTorque(lua_State* L) {
    check(L).setBreakTorque(lua::Stack::read<std::optional<float>>(L, 3));
    return 0;
}

int JointBinding::getTarget(lua_State* L) {
    lua::Stack::push(L, check(L).getTarget());
    return 1;
}

int JointBinding::setTarget(lua_State* L) {
    check(L).setTarget(lua::Stack::read<haylen::math::Vec2>(L, 3));
    return 0;
}

int JointBinding::getAngle(lua_State* L) {
    lua::Stack::push(L, check(L).getAngle());
    return 1;
}

int JointBinding::getTranslation(lua_State* L) {
    lua::Stack::push(L, check(L).getTranslation());
    return 1;
}

int JointBinding::getCurrentLength(lua_State* L) {
    lua::Stack::push(L, check(L).getCurrentLength());
    return 1;
}

int JointBinding::isLimitEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isLimitEnabled());
    return 1;
}

int JointBinding::setLimitEnabled(lua_State* L) {
    check(L).setLimitEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointBinding::getLower(lua_State* L) {
    lua::Stack::push(L, check(L).getLower());
    return 1;
}

int JointBinding::setLower(lua_State* L) {
    haylen::physics2d::Joint& joint = check(L);
    joint.setLimits(lua::Stack::read<float>(L, 3), joint.getUpper());
    return 0;
}

int JointBinding::getUpper(lua_State* L) {
    lua::Stack::push(L, check(L).getUpper());
    return 1;
}

int JointBinding::setUpper(lua_State* L) {
    haylen::physics2d::Joint& joint = check(L);
    joint.setLimits(joint.getLower(), lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::isMotorEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isMotorEnabled());
    return 1;
}

int JointBinding::setMotorEnabled(lua_State* L) {
    check(L).setMotorEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointBinding::getMotorSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorSpeed());
    return 1;
}

int JointBinding::setMotorSpeed(lua_State* L) {
    check(L).setMotorSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getMaxMotorForce(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxMotorForce());
    return 1;
}

int JointBinding::setMaxMotorForce(lua_State* L) {
    check(L).setMaxMotorForce(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getMaxMotorTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getMaxMotorTorque());
    return 1;
}

int JointBinding::setMaxMotorTorque(lua_State* L) {
    check(L).setMaxMotorTorque(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getMotorForce(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorForce());
    return 1;
}

int JointBinding::getMotorTorque(lua_State* L) {
    lua::Stack::push(L, check(L).getMotorTorque());
    return 1;
}

int JointBinding::isSpringEnabled(lua_State* L) {
    lua::Stack::push(L, check(L).isSpringEnabled());
    return 1;
}

int JointBinding::setSpringEnabled(lua_State* L) {
    check(L).setSpringEnabled(lua::Stack::read<bool>(L, 3));
    return 0;
}

int JointBinding::getHertz(lua_State* L) {
    lua::Stack::push(L, check(L).getHertz());
    return 1;
}

int JointBinding::setHertz(lua_State* L) {
    check(L).setHertz(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getDampingRatio(lua_State* L) {
    lua::Stack::push(L, check(L).getDampingRatio());
    return 1;
}

int JointBinding::setDampingRatio(lua_State* L) {
    check(L).setDampingRatio(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getLength(lua_State* L) {
    lua::Stack::push(L, check(L).getLength());
    return 1;
}

int JointBinding::setLength(lua_State* L) {
    check(L).setLength(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getTargetAngle(lua_State* L) {
    lua::Stack::push(L, check(L).getTargetAngle());
    return 1;
}

int JointBinding::setTargetAngle(lua_State* L) {
    check(L).setTargetAngle(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getTargetTranslation(lua_State* L) {
    lua::Stack::push(L, check(L).getTargetTranslation());
    return 1;
}

int JointBinding::setTargetTranslation(lua_State* L) {
    check(L).setTargetTranslation(lua::Stack::read<float>(L, 3));
    return 0;
}

int JointBinding::getLinearOffset(lua_State* L) {
    lua::Stack::push(L, check(L).getLinearOffset());
    return 1;
}

int JointBinding::setLinearOffset(lua_State* L) {
    check(L).setLinearOffset(lua::Stack::read<haylen::math::Vec2>(L, 3));
    return 0;
}

int JointBinding::getAngularOffset(lua_State* L) {
    lua::Stack::push(L, check(L).getAngularOffset());
    return 1;
}

int JointBinding::setAngularOffset(lua_State* L) {
    check(L).setAngularOffset(lua::Stack::read<float>(L, 3));
    return 0;
}

void JointBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedHandle<haylen::physics2d::Joint>>(L).function("destroy", &lua::Binding::native<&destroy>).property("valid", &isValid).property("bodyA", &lua::Binding::native<&getBodyA>).property("bodyB", &lua::Binding::native<&getBodyB>).property("anchorA", &lua::Binding::native<&getAnchorA>).property("anchorB", &lua::Binding::native<&getAnchorB>).property("type", &lua::Binding::native<&getType>).property("constraintForce", &lua::Binding::native<&getConstraintForce>).property("constraintTorque", &lua::Binding::native<&getConstraintTorque>).property("linearSeparation", &lua::Binding::native<&getLinearSeparation>).property("angularSeparation", &lua::Binding::native<&getAngularSeparation>).property("constraintHertz", &lua::Binding::native<&getConstraintHertz>, &lua::Binding::native<&setConstraintHertz>).property("constraintDampingRatio", &lua::Binding::native<&getConstraintDampingRatio>, &lua::Binding::native<&setConstraintDampingRatio>).property("breakForce", &lua::Binding::native<&getBreakForce>, &lua::Binding::native<&setBreakForce>).property("breakTorque", &lua::Binding::native<&getBreakTorque>, &lua::Binding::native<&setBreakTorque>).property("target", &lua::Binding::native<&getTarget>, &lua::Binding::native<&setTarget>).property("angle", &lua::Binding::native<&getAngle>).property("translation", &lua::Binding::native<&getTranslation>).property("currentLength", &lua::Binding::native<&getCurrentLength>).property("enableLimit", &lua::Binding::native<&isLimitEnabled>, &lua::Binding::native<&setLimitEnabled>).property("lower", &lua::Binding::native<&getLower>, &lua::Binding::native<&setLower>).property("upper", &lua::Binding::native<&getUpper>, &lua::Binding::native<&setUpper>).property("enableMotor", &lua::Binding::native<&isMotorEnabled>, &lua::Binding::native<&setMotorEnabled>).property("motorSpeed", &lua::Binding::native<&getMotorSpeed>, &lua::Binding::native<&setMotorSpeed>).property("maxMotorForce", &lua::Binding::native<&getMaxMotorForce>, &lua::Binding::native<&setMaxMotorForce>).property("maxMotorTorque", &lua::Binding::native<&getMaxMotorTorque>, &lua::Binding::native<&setMaxMotorTorque>).property("motorForce", &lua::Binding::native<&getMotorForce>).property("motorTorque", &lua::Binding::native<&getMotorTorque>).property("enableSpring", &lua::Binding::native<&isSpringEnabled>, &lua::Binding::native<&setSpringEnabled>).property("hertz", &lua::Binding::native<&getHertz>, &lua::Binding::native<&setHertz>).property("dampingRatio", &lua::Binding::native<&getDampingRatio>, &lua::Binding::native<&setDampingRatio>).property("length", &lua::Binding::native<&getLength>, &lua::Binding::native<&setLength>).property("targetAngle", &lua::Binding::native<&getTargetAngle>, &lua::Binding::native<&setTargetAngle>).property("targetTranslation", &lua::Binding::native<&getTargetTranslation>, &lua::Binding::native<&setTargetTranslation>).property("linearOffset", &lua::Binding::native<&getLinearOffset>, &lua::Binding::native<&setLinearOffset>).property("angularOffset", &lua::Binding::native<&getAngularOffset>, &lua::Binding::native<&setAngularOffset>).meta("__eq", &equal).install();
}

} // namespace haylen::lua::physics2d
