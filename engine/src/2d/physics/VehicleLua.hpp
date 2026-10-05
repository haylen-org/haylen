#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

class TopDownVehicle;
class Vehicle;

// Installs the `Vehicle` and `TopDownVehicle` classes of `haylen.physics2d`: a car seen from the side on springy suspension and a car seen from above with tires that grip and drift. Both own their bodies, which go with them.
class VehicleLua final {
  public:
    static void install(lua_State* L);

    // Sets `newVehicle` and `newTopDownVehicle` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 24> kVehicleFields{"x", "y", "chassisWidth", "chassisHeight", "wheelRadius", "rearWheel", "frontWheel", "density", "wheelDensity", "wheelFriction", "suspensionHertz", "suspensionDamping", "suspensionTravel", "acceleration", "topSpeed", "brakeAcceleration", "centerOfMass", "airControl", "antiRoll", "drive", "category", "mask", "group", "bullet"};
    static constexpr std::array<std::string_view, 27> kTopDownFields{"x", "y", "rotation", "length", "width", "density", "friction", "restitution", "frontAxle", "rearAxle", "acceleration", "reverseAcceleration", "topSpeed", "reverseSpeed", "brakeAcceleration", "grip", "handbrakeGrip", "steeringLock", "steeringSpeed", "highSpeedLock", "rollingDrag", "angularDamping", "drive", "category", "mask", "group", "bullet"};

    [[nodiscard]] static Vehicle& checkVehicle(lua_State* L);
    [[nodiscard]] static TopDownVehicle& checkTopDown(lua_State* L);

    static int newVehicle(lua_State* L);
    static int vehicleChassis(lua_State* L);
    static int vehicleRearWheel(lua_State* L);
    static int vehicleFrontWheel(lua_State* L);
    static int vehicleJoints(lua_State* L);
    static int vehicleGetThrottle(lua_State* L);
    static int vehicleSetThrottle(lua_State* L);
    static int vehicleGetBrake(lua_State* L);
    static int vehicleSetBrake(lua_State* L);
    static int vehicleGrounded(lua_State* L);
    static int vehicleSpeed(lua_State* L);
    static int vehicleDrive(lua_State* L);
    static int vehicleDestroy(lua_State* L);
    static int vehicleValid(lua_State* L);

    static int newTopDownVehicle(lua_State* L);
    static int topDownBody(lua_State* L);
    static int topDownGetThrottle(lua_State* L);
    static int topDownSetThrottle(lua_State* L);
    static int topDownGetSteering(lua_State* L);
    static int topDownSetSteering(lua_State* L);
    static int topDownGetBrake(lua_State* L);
    static int topDownSetBrake(lua_State* L);
    static int topDownGetHandbrake(lua_State* L);
    static int topDownSetHandbrake(lua_State* L);
    static int topDownSteeringAngle(lua_State* L);
    static int topDownSpeed(lua_State* L);
    static int topDownSlip(lua_State* L);
    static int topDownDrifting(lua_State* L);
    static int topDownDestroy(lua_State* L);
    static int topDownValid(lua_State* L);
};

} // namespace haylen::physics2d
