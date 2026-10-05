#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/ContactEvent.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/2d/physics/SensorEvent.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/2d/physics/World.hpp"

struct lua_State;

namespace haylen::physics2d {

// Installs the `PhysicsWorld` class of `haylen.physics2d`. The user value of a world object is its state table, which holds the event callbacks and the data of its bodies.
class WorldLua final {
  public:
    static void install(lua_State* L);

    // Reads `{type, x, y, rotation, vx, vy, angularVelocity, linearDamping, angularDamping, gravityScale, fixedRotation, bullet, fastRotation, sleepEnabled, sleepThreshold}`, where `nil` gives the defaults.
    [[nodiscard]] static Body::Options readBodyOptions(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 15> kBodyFields{"type", "x", "y", "rotation", "vx", "vy", "angularVelocity", "linearDamping", "angularDamping", "gravityScale", "fixedRotation", "bullet", "fastRotation", "sleepEnabled", "sleepThreshold"};
    static constexpr std::array<std::string_view, 22> kJointFields{"ax", "ay", "bx", "by", "collideConnected", "enableLimit", "lower", "upper", "enableMotor", "motorSpeed", "maxMotorForce", "maxMotorTorque", "enableSpring", "hertz", "dampingRatio", "targetAngle", "targetTranslation", "axisX", "axisY", "length", "breakForce", "breakTorque"};
    static constexpr std::array<std::string_view, 2> kFilterFields{"category", "mask"};
    static constexpr std::array<std::string_view, 6> kPredictFields{"steps", "step", "radius", "gravityScale", "linearDamping", "group"};
    static constexpr std::array<std::string_view, 6> kCallbacks{"onContactBegin", "onContactEnd", "onHit", "onSensorBegin", "onSensorEnd", "onJointBreak"};

    [[nodiscard]] static World& check(lua_State* L);
    [[nodiscard]] static Joint::Options readJointOptions(lua_State* L, int index);
    [[nodiscard]] static CollisionFilter readQueryFilter(lua_State* L, int index);
    static void pushContact(lua_State* L, int worldIndex, const ContactEvent& event);
    static void pushBodyOf(lua_State* L, int worldIndex, const Shape& shape);

    // Fetches the named callback from the world state, leaving it on the stack when it exists.
    [[nodiscard]] static bool pushCallback(lua_State* L, int worldIndex, const char* name);
    [[nodiscard]] static bool hasCallback(lua_State* L, int worldIndex, const char* name);
    static void releaseDestroyedData(lua_State* L, int worldIndex);
    static void dispatchContacts(lua_State* L, int worldIndex, const char* name, const std::vector<ContactEvent>& events);
    static void dispatchSensors(lua_State* L, int worldIndex, const char* name, const std::vector<SensorEvent>& events);
    static void dispatchJointBreaks(lua_State* L, int worldIndex, const std::vector<World::JointBreak>& breaks);

    // Reads a list of bodies and the float buffer after it, whose values start at the optional one-based position after the buffer.
    [[nodiscard]] static std::vector<Body> readBodies(lua_State* L, int index);
    [[nodiscard]] static std::span<float> readTransformValues(lua_State* L, int index);

    static int createBody(lua_State* L);
    static int createJoint(lua_State* L);
    static int readTransforms(lua_State* L);
    static int writeTransforms(lua_State* L);
    static int step(lua_State* L);
    static int wakeAll(lua_State* L);
    static int stats(lua_State* L);
    static int stateHash(lua_State* L);
    static int predictPath(lua_State* L);
    static int queryRect(lua_State* L);
    static int queryCircle(lua_State* L);
    static int queryPoint(lua_State* L);
    static int debugDraw(lua_State* L);
    static int getGravity(lua_State* L);
    static int setGravity(lua_State* L);
    static int getBodyCount(lua_State* L);
    static int getAwakeBodyCount(lua_State* L);
    static int getPixelsPerMeter(lua_State* L);
    static int getThreads(lua_State* L);
    static int isInterpolating(lua_State* L);
    static int getSubSteps(lua_State* L);
    static int setSubSteps(lua_State* L);
    static int isContinuous(lua_State* L);
    static int setContinuous(lua_State* L);
    static int isSleepEnabled(lua_State* L);
    static int setSleepEnabled(lua_State* L);
    static int getMaxSpeed(lua_State* L);
    static int setMaxSpeed(lua_State* L);
    static int getContactHertz(lua_State* L);
    static int setContactHertz(lua_State* L);
    static int getContactDampingRatio(lua_State* L);
    static int setContactDampingRatio(lua_State* L);
    static int getContactPushSpeed(lua_State* L);
    static int setContactPushSpeed(lua_State* L);
    static int getRestitutionThreshold(lua_State* L);
    static int setRestitutionThreshold(lua_State* L);
    static int getHitThreshold(lua_State* L);
    static int setHitThreshold(lua_State* L);
    template <std::size_t Index> static int getCallback(lua_State* L);
    template <std::size_t Index> static int setCallback(lua_State* L);
};

} // namespace haylen::physics2d
