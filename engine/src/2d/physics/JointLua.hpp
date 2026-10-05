#pragma once

struct lua_State;

namespace haylen::physics2d {

class Joint;

// Installs the `Joint` class of `haylen.physics2d`, whose properties read and change every setting a joint has at creation and read what it does while it runs.
class JointLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static Joint& check(lua_State* L);

    static int isValid(lua_State* L);
    static int destroy(lua_State* L);
    static int getBodyA(lua_State* L);
    static int getBodyB(lua_State* L);
    static int getAnchorA(lua_State* L);
    static int getAnchorB(lua_State* L);
    static int equal(lua_State* L);
    static int getType(lua_State* L);
    static int getConstraintForce(lua_State* L);
    static int getConstraintTorque(lua_State* L);
    static int getLinearSeparation(lua_State* L);
    static int getAngularSeparation(lua_State* L);
    static int getConstraintHertz(lua_State* L);
    static int setConstraintHertz(lua_State* L);
    static int getConstraintDampingRatio(lua_State* L);
    static int setConstraintDampingRatio(lua_State* L);
    static int getBreakForce(lua_State* L);
    static int setBreakForce(lua_State* L);
    static int getBreakTorque(lua_State* L);
    static int setBreakTorque(lua_State* L);
    static int getTarget(lua_State* L);
    static int setTarget(lua_State* L);
    static int getAngle(lua_State* L);
    static int getTranslation(lua_State* L);
    static int getCurrentLength(lua_State* L);
    static int isLimitEnabled(lua_State* L);
    static int setLimitEnabled(lua_State* L);
    static int getLower(lua_State* L);
    static int setLower(lua_State* L);
    static int getUpper(lua_State* L);
    static int setUpper(lua_State* L);
    static int isMotorEnabled(lua_State* L);
    static int setMotorEnabled(lua_State* L);
    static int getMotorSpeed(lua_State* L);
    static int setMotorSpeed(lua_State* L);
    static int getMaxMotorForce(lua_State* L);
    static int setMaxMotorForce(lua_State* L);
    static int getMaxMotorTorque(lua_State* L);
    static int setMaxMotorTorque(lua_State* L);
    static int getMotorForce(lua_State* L);
    static int getMotorTorque(lua_State* L);
    static int isSpringEnabled(lua_State* L);
    static int setSpringEnabled(lua_State* L);
    static int getHertz(lua_State* L);
    static int setHertz(lua_State* L);
    static int getDampingRatio(lua_State* L);
    static int setDampingRatio(lua_State* L);
    static int getLength(lua_State* L);
    static int setLength(lua_State* L);
    static int getTargetAngle(lua_State* L);
    static int setTargetAngle(lua_State* L);
    static int getTargetTranslation(lua_State* L);
    static int setTargetTranslation(lua_State* L);
    static int getLinearOffset(lua_State* L);
    static int setLinearOffset(lua_State* L);
    static int getAngularOffset(lua_State* L);
    static int setAngularOffset(lua_State* L);
};

} // namespace haylen::physics2d
