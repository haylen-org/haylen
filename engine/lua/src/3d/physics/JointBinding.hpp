#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string_view>

#include "haylen/3d/physics/Joint.hpp"
#include "haylen/3d/physics/World.hpp"

namespace haylen::lua::physics3d {

// A joint held by Lua, which keeps its world alive. Its user value is the Lua world object.
struct ScriptedJoint {
    std::shared_ptr<haylen::physics3d::World> world;
    haylen::physics3d::Joint joint;
};

// Installs the `Joint3D` class of `haylen.physics3d` and reads the options that `world:createJoint` takes, whose springs, motors and axes are tables.
class JointBinding final {
  public:
    static void install(lua_State* L);

    // Pushes a joint of the Lua world object at `worldIndex`.
    static void push(lua_State* L, int worldIndex, const haylen::physics3d::Joint& joint);

    [[nodiscard]] static haylen::physics3d::Joint::Options readOptions(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 30> kOptionFields{"point", "secondPoint", "axis", "normal", "secondAxis", "lower", "upper", "limitSpring", "friction", "halfConeAngle", "planeHalfConeAngle", "motor", "swingMotor", "targetRotation", "targetAngularVelocity", "axes", "path", "pathRotation", "pathFraction", "ratio", "joints", "fixedPoint", "secondFixedPoint", "priority", "velocitySteps", "positionSteps", "collideConnected", "enabled", "breakForce", "breakTorque"};
    static constexpr std::array<std::string_view, 4> kSpringFields{"mode", "frequency", "stiffness", "damping"};
    static constexpr std::array<std::string_view, 6> kMotorFields{"state", "targetVelocity", "targetPosition", "maxForce", "maxTorque", "spring"};
    static constexpr std::array<std::string_view, 6> kAxisFields{"mode", "lower", "upper", "limitSpring", "friction", "motor"};

    [[nodiscard]] static haylen::physics3d::Joint& check(lua_State* L, int index);
    [[nodiscard]] static haylen::physics3d::Joint::Spring readSpring(lua_State* L, int index);
    [[nodiscard]] static haylen::physics3d::Joint::Motor readMotor(lua_State* L, int index);
    [[nodiscard]] static haylen::physics3d::Joint::AxisSettings readAxis(lua_State* L, int index);
    static void pushSpring(lua_State* L, const haylen::physics3d::Joint::Spring& spring);
    static void pushMotor(lua_State* L, const haylen::physics3d::Joint::Motor& motor);
    static void pushAxis(lua_State* L, const haylen::physics3d::Joint::AxisSettings& axis);

    // The motor slot that the axis of a six degrees of freedom joint drives.
    [[nodiscard]] static haylen::physics3d::Joint::MotorSlot slotOf(haylen::physics3d::Joint::Axis axis) noexcept;

    static int isValid(lua_State* L);
    static int getType(lua_State* L);
    static int getFirst(lua_State* L);
    static int getSecond(lua_State* L);
    static int isEnabled(lua_State* L);
    static int setEnabled(lua_State* L);
    static int getPriority(lua_State* L);
    static int setPriority(lua_State* L);
    static int getForce(lua_State* L);
    static int getTorque(lua_State* L);
    static int getBreakForce(lua_State* L);
    static int setBreakForce(lua_State* L);
    static int getBreakTorque(lua_State* L);
    static int setBreakTorque(lua_State* L);
    static int getLower(lua_State* L);
    static int getUpper(lua_State* L);
    static int setLimits(lua_State* L);
    static int getLimitSpring(lua_State* L);
    static int setLimitSpring(lua_State* L);
    static int getFriction(lua_State* L);
    static int setFriction(lua_State* L);
    static int getHalfConeAngle(lua_State* L);
    static int setHalfConeAngle(lua_State* L);
    static int getPlaneHalfConeAngle(lua_State* L);
    static int setPlaneHalfConeAngle(lua_State* L);
    static int getRatio(lua_State* L);
    static int getAngle(lua_State* L);
    static int getPosition(lua_State* L);
    static int getLength(lua_State* L);
    static int getFraction(lua_State* L);
    static int getMotor(lua_State* L);
    static int setMotor(lua_State* L);
    static int getSwingMotor(lua_State* L);
    static int setSwingMotor(lua_State* L);
    static int getTargetRotation(lua_State* L);
    static int setTargetRotation(lua_State* L);
    static int getTargetAngularVelocity(lua_State* L);
    static int setTargetAngularVelocity(lua_State* L);
    static int getAxis(lua_State* L);
    static int setAxis(lua_State* L);
    static int destroy(lua_State* L);
    static int equal(lua_State* L);
};

} // namespace haylen::lua::physics3d
