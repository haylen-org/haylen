#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "haylen/3d/physics/Body.hpp"
#include "haylen/3d/physics/CollisionFilter.hpp"
#include "haylen/3d/physics/Joint.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"
#include "haylen/3d/physics/WorldStats.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

// The simulation of one 3D physics world, which a backend makes for `physics3d::World`. The world checks every value before it reaches the simulation and calls it on the thread that steps the world alone, so a simulation trusts its input and needs no lock of its own.
class Simulation {
  public:
    // The id of a body names its slot below `maxBodies` and the generation of the slot, so the id of a destroyed body never names the body that takes its slot.
    using BodyId = std::uint32_t;

    // The id of a joint names its slot and the generation of the slot, as the id of a body does.
    using JointId = std::uint32_t;

    // The limits that a step ran into, whose contacts or temporary data the step left out, which the world reports once each.
    struct StepReport {
        bool bodyPairsFull = false;
        bool contactConstraintsFull = false;
        bool temporaryMemoryFull = false;
    };

    Simulation() = default;
    virtual ~Simulation() = default;

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    // Advances the world by the time in the collision steps of the settings. A step allocates nothing.
    [[nodiscard]] virtual StepReport step(float deltaSeconds) = 0;

    // Takes the settings that may change while the world lives: the gravity, the steps, the sleep and the tuning of contacts. The limits keep the values the simulation was made with.
    virtual void configure(const WorldSettings& settings) = 0;

    [[nodiscard]] virtual WorldStats getStats() const = 0;

    // Hashes the whole state that the simulation advances bit for bit, so two runs that should match compare one number.
    [[nodiscard]] virtual std::uint64_t computeStateHash() const = 0;

    // Makes a body of options that the world checked, whose unset internal edge removal it set, and returns its id, or nothing when the world holds `maxBodies` already.
    [[nodiscard]] virtual std::optional<BodyId> createBody(const Body::Options& options) = 0;
    virtual void destroyBody(BodyId id) = 0;
    [[nodiscard]] virtual bool isBodyValid(BodyId id) const noexcept = 0;

    // The slot of a body below `maxBodies`, which the world keeps per-body data at.
    [[nodiscard]] virtual std::size_t getBodySlot(BodyId id) const noexcept = 0;

    [[nodiscard]] virtual Body::Type getBodyType(BodyId id) const = 0;
    virtual void setBodyType(BodyId id, Body::Type value) = 0;
    [[nodiscard]] virtual Body::MotionQuality getMotionQuality(BodyId id) const = 0;
    virtual void setMotionQuality(BodyId id, Body::MotionQuality value) = 0;
    [[nodiscard]] virtual Body::MotionLocks getMotionLocks(BodyId id) const = 0;
    virtual void setMotionLocks(BodyId id, const Body::MotionLocks& value) = 0;

    [[nodiscard]] virtual Body::Transform getBodyTransform(BodyId id) const = 0;
    virtual void setBodyTransform(BodyId id, const Body::Transform& value) = 0;
    virtual void moveBody(BodyId id, const Body::Transform& target, float seconds) = 0;

    // Copies the transforms of the bodies into seven floats each, `x`, `y`, `z` and the `x`, `y`, `z` and `w` of the rotation, or moves the bodies to them.
    virtual void readBodyTransforms(std::span<const BodyId> ids, std::span<float> values) const = 0;
    virtual void writeBodyTransforms(std::span<const BodyId> ids, std::span<const float> values) = 0;

    [[nodiscard]] virtual math::Vec3 getBodyVector(BodyId id, Body::Vector which) const = 0;
    virtual void setBodyVector(BodyId id, Body::Vector which, math::Vec3 value) = 0;
    [[nodiscard]] virtual float getBodyValue(BodyId id, Body::Value which) const = 0;
    virtual void setBodyValue(BodyId id, Body::Value which, float value) = 0;
    [[nodiscard]] virtual bool getBodyFlag(BodyId id, Body::Flag which) const = 0;
    virtual void setBodyFlag(BodyId id, Body::Flag which, bool value) = 0;
    [[nodiscard]] virtual Body::CombineRule getCombineRule(BodyId id, Body::Material which) const = 0;
    virtual void setCombineRule(BodyId id, Body::Material which, Body::CombineRule value) = 0;

    // Forgets the mass, inertia and center of mass that were set, so the body takes them from its shape again.
    virtual void resetMass(BodyId id) = 0;

    [[nodiscard]] virtual math::Vec3 getVelocityAt(BodyId id, math::Vec3 point) const = 0;
    virtual void pushBody(BodyId id, Body::Push which, math::Vec3 value, std::optional<math::Vec3> point) = 0;
    [[nodiscard]] virtual math::Box getBodyBounds(BodyId id) const = 0;

    [[nodiscard]] virtual CollisionFilter getBodyFilter(BodyId id) const = 0;
    virtual void setBodyFilter(BodyId id, const CollisionFilter& value) = 0;

    // The shape the body took, or its own copy of a mutable compound.
    [[nodiscard]] virtual std::shared_ptr<const Shape> getBodyShape(BodyId id) const = 0;
    virtual void setBodyShape(BodyId id, const std::shared_ptr<const Shape>& value) = 0;

    // The children of the own copy of a mutable compound of a body, which the world checked to be one, with an index the world checked.
    [[nodiscard]] virtual std::size_t getChildCount(BodyId id) const = 0;
    virtual std::size_t addChild(BodyId id, const Shape::Child& child) = 0;
    virtual void removeChild(BodyId id, std::size_t index) = 0;
    virtual void setChildTransform(BodyId id, std::size_t index, const Body::Transform& value) = 0;

    // Makes a joint of options that the world checked, between the first body and the second one or the world, whose `joints` the world checked to be live joints of the right types. A body that is destroyed takes its joints with it.
    [[nodiscard]] virtual JointId createJoint(Joint::Type type, BodyId first, std::optional<BodyId> second, const Joint::Options& options) = 0;
    virtual void destroyJoint(JointId id) = 0;
    [[nodiscard]] virtual bool isJointValid(JointId id) const noexcept = 0;
    [[nodiscard]] virtual Joint::Type getJointType(JointId id) const = 0;
    [[nodiscard]] virtual std::pair<BodyId, std::optional<BodyId>> getJointBodies(JointId id) const = 0;
    [[nodiscard]] virtual bool isJointEnabled(JointId id) const = 0;
    virtual void setJointEnabled(JointId id, bool value) = 0;
    [[nodiscard]] virtual int getJointPriority(JointId id) const = 0;
    virtual void setJointPriority(JointId id, int value) = 0;

    // The force and torque that held the joint in the last step.
    [[nodiscard]] virtual Joint::Load getJointLoad(JointId id) const = 0;

    // The world checked that the joint has the value, the limits, the spring, the motor, the target or the axis, and the value itself.
    [[nodiscard]] virtual float getJointValue(JointId id, Joint::Value which) const = 0;
    virtual void setJointValue(JointId id, Joint::Value which, float value) = 0;
    virtual void setJointLimits(JointId id, float lower, float upper) = 0;
    [[nodiscard]] virtual Joint::Spring getLimitSpring(JointId id) const = 0;
    virtual void setLimitSpring(JointId id, const Joint::Spring& value) = 0;
    [[nodiscard]] virtual Joint::Motor getJointMotor(JointId id, Joint::MotorSlot slot) const = 0;
    virtual void setJointMotor(JointId id, Joint::MotorSlot slot, const Joint::Motor& value) = 0;
    [[nodiscard]] virtual math::Quat getTargetRotation(JointId id) const = 0;
    virtual void setTargetRotation(JointId id, const math::Quat& value) = 0;
    [[nodiscard]] virtual math::Vec3 getTargetAngularVelocity(JointId id) const = 0;
    virtual void setTargetAngularVelocity(JointId id, math::Vec3 value) = 0;
    [[nodiscard]] virtual Joint::AxisSettings getJointAxis(JointId id, Joint::Axis axis) const = 0;
    virtual void setJointAxis(JointId id, Joint::Axis axis, const Joint::AxisSettings& value) = 0;

    // Fills `ids` with the bodies that are awake, in the storage it holds, so a world that keeps the vector allocates nothing once it grew.
    virtual void collectActiveBodies(std::vector<BodyId>& ids) const = 0;
};

} // namespace haylen::physics3d
