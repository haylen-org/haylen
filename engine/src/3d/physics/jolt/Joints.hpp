#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "3d/physics/jolt/ContactFilter.hpp"
#include "haylen/3d/physics/Joint.hpp"
#include "haylen/3d/physics/Simulation.hpp"

namespace haylen::physics3d::jolt {

// The joints of a world, each a constraint of Jolt in a slot with a generation, so the id of a destroyed joint never names the joint that takes its slot. A joint whose bodies do not collide with each other lists its pair in the contact filter of the world.
class Joints final {
  public:
    using BodyId = physics3d::Simulation::BodyId;
    using JointId = physics3d::Simulation::JointId;

    Joints(JPH::PhysicsSystem& physics, ContactFilter& filter) noexcept : system(physics), contacts(filter) {}

    Joints(const Joints&) = delete;
    Joints& operator=(const Joints&) = delete;

    [[nodiscard]] JointId create(Joint::Type type, BodyId first, std::optional<BodyId> second, const Joint::Options& options);
    void destroy(JointId id);

    // Destroys every joint that holds the body, before the body goes.
    void destroyJointsOf(BodyId body);

    [[nodiscard]] bool isValid(JointId id) const noexcept;
    [[nodiscard]] Joint::Type getType(JointId id) const;
    [[nodiscard]] std::pair<BodyId, std::optional<BodyId>> getBodies(JointId id) const;
    [[nodiscard]] bool isEnabled(JointId id) const;
    void setEnabled(JointId id, bool value);
    [[nodiscard]] int getPriority(JointId id) const;
    void setPriority(JointId id, int value);

    // The load of the last step, from the impulses the solver gave the joint in the last collision step, whose time is `subStep`.
    [[nodiscard]] Joint::Load getLoad(JointId id, float subStep) const;

    [[nodiscard]] float getValue(JointId id, Joint::Value which) const;
    void setValue(JointId id, Joint::Value which, float value);
    void setLimits(JointId id, float lower, float upper);
    [[nodiscard]] Joint::Spring getLimitSpring(JointId id) const;
    void setLimitSpring(JointId id, const Joint::Spring& value);
    [[nodiscard]] Joint::Motor getMotor(JointId id, Joint::MotorSlot slot) const;
    void setMotor(JointId id, Joint::MotorSlot slot, const Joint::Motor& value);
    [[nodiscard]] math::Quat getTargetRotation(JointId id) const;
    void setTargetRotation(JointId id, const math::Quat& value);
    [[nodiscard]] math::Vec3 getTargetAngularVelocity(JointId id) const;
    void setTargetAngularVelocity(JointId id, math::Vec3 value);
    [[nodiscard]] Joint::AxisSettings getAxis(JointId id, Joint::Axis axis) const;
    void setAxis(JointId id, Joint::Axis axis, const Joint::AxisSettings& value);

  private:
    // A joint in its slot: the constraint, the bodies it holds, and the motors, springs, targets and axes as the app set them, which the constraint follows.
    struct Record {
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        Joint::Type type = Joint::Type::Fixed;
        BodyId first = 0;
        std::optional<BodyId> second;
        bool collideConnected = false;
        std::uint8_t generation = 0;
        Joint::Motor main{};
        Joint::Motor swing{};
        std::array<Joint::AxisSettings, 6> axes{};
        Joint::Spring limitSpring{};
        math::Quat targetRotation = math::Quat::identity();
        math::Vec3 targetAngularVelocity{};
        float ratio = 1.0F;
    };

    static constexpr JointId kIndexBits = 24;
    static constexpr JointId kIndexMask = (JointId{1} << kIndexBits) - 1;

    // A path takes this many samples for each segment of its spline, within these bounds.
    static constexpr std::size_t kSamplesPerSegment = 16;
    static constexpr std::size_t kMinPathSamples = 8;
    static constexpr std::size_t kMaxPathSamples = 1024;

    [[nodiscard]] static JPH::SpringSettings toJolt(const Joint::Spring& value) noexcept;
    [[nodiscard]] static JPH::MotorSettings toJolt(const Joint::Motor& value) noexcept;
    [[nodiscard]] static JPH::EMotorState toJolt(Joint::MotorState value) noexcept;
    [[nodiscard]] static JPH::EPathRotationConstraintType toJolt(Joint::PathRotation value) noexcept;
    [[nodiscard]] static JPH::Ref<JPH::PathConstraintPath> makePath(const math::Spline3D& spline, const JPH::Body& first);

    [[nodiscard]] const Record& getRecord(JointId id) const;
    [[nodiscard]] Record& getRecord(JointId id);
    [[nodiscard]] JPH::Body& getBody(std::optional<BodyId> id) const;

    // The targets of three axes of a six degrees of freedom joint from the first, and their limits.
    [[nodiscard]] static JPH::Vec3 gatherTargets(const Record& record, std::size_t first, bool velocity) noexcept;
    [[nodiscard]] static float limitOf(const Joint::AxisSettings& settings, bool lower) noexcept;
    [[nodiscard]] static JPH::Vec3 gatherLimits(const Record& record, std::size_t first, bool lower) noexcept;

    [[nodiscard]] static float getLimit(const Record& record, bool lower);
    [[nodiscard]] static float getFriction(const Record& record);

    // Gives the constraint the state and targets of the motor in the slot.
    void applyMotor(Record& record, Joint::MotorSlot slot);

    JPH::PhysicsSystem& system;
    ContactFilter& contacts;
    std::vector<Record> records;
    std::vector<JointId> freeSlots;
};

} // namespace haylen::physics3d::jolt
