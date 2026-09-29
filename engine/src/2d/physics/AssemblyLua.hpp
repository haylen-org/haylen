#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::physics2d {

// Installs the Rope, Ragdoll and Vehicle classes of haylen.physics2d, which build groups of bodies and joints in a world.
class AssemblyLua final {
  public:
    static void install(lua_State* L);

    // Sets newRope, newBridge, newRagdoll and newVehicle on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 16> kRopeFields{"from", "to", "segments", "thickness", "density", "friction", "linearDamping", "angularDamping", "planks", "pinStart", "pinEnd", "startBody", "endBody", "category", "mask", "group"};
    static constexpr std::array<std::string_view, 9> kRagdollFields{"x", "y", "height", "density", "friction", "jointFriction", "group", "vx", "vy"};
    static constexpr std::array<std::string_view, 16> kVehicleFields{"x", "y", "chassisWidth", "chassisHeight", "wheelRadius", "rearWheel", "frontWheel", "density", "wheelDensity", "wheelFriction", "suspensionHertz", "suspensionDamping", "suspensionTravel", "maxMotorTorque", "drive", "group"};

    // Pushes the Lua world object of the assembly at index 1.
    static void pushWorld(lua_State* L);
    static int createRope(lua_State* L, bool bridge);

    static int newRope(lua_State* L);
    static int newBridge(lua_State* L);
    static int ropeBodies(lua_State* L);
    static int ropeJoints(lua_State* L);
    static int ropeSegments(lua_State* L);
    static int ropePoints(lua_State* L);
    static int ropeDestroy(lua_State* L);
    static int ropeValid(lua_State* L);
    static int ropeSegmentLength(lua_State* L);

    static int newRagdoll(lua_State* L);
    static int ragdollBody(lua_State* L);
    static int ragdollBodies(lua_State* L);
    static int ragdollJoints(lua_State* L);
    static int ragdollDestroy(lua_State* L);
    static int ragdollValid(lua_State* L);

    static int newVehicle(lua_State* L);
    static int vehicleChassis(lua_State* L);
    static int vehicleRearWheel(lua_State* L);
    static int vehicleFrontWheel(lua_State* L);
    static int vehicleJoints(lua_State* L);
    static int vehicleGetMotorSpeed(lua_State* L);
    static int vehicleSetMotorSpeed(lua_State* L);
    static int vehicleDrive(lua_State* L);
    static int vehicleDestroy(lua_State* L);
    static int vehicleValid(lua_State* L);
};

} // namespace haylen::physics2d
