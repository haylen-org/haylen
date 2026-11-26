#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>
#include <optional>
#include <vector>

#include "3d/physics/jolt/ContactFilter.hpp"
#include "3d/physics/jolt/Joints.hpp"
#include "3d/physics/jolt/Layers.hpp"
#include "3d/physics/jolt/StepAllocator.hpp"
#include "3d/physics/jolt/StepJobs.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/WorldSettings.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics3d::jolt {

// The simulation of a world by a physics system of Jolt. Every body may change its type, so each one keeps the motion of a moving body, and the id of a body is the id of Jolt with its sequence.
class Simulation final : public physics3d::Simulation {
  public:
    // A world steps on its threads only while this many bodies are awake, because below it waking the workers costs more than they save.
    static constexpr JPH::uint kParallelBodies = 1000;

    Simulation(const WorldSettings& settings, core::JobSystem* jobSystem);
    ~Simulation() override;

    [[nodiscard]] StepReport step(float deltaSeconds) override;
    void configure(const WorldSettings& settings) override;
    [[nodiscard]] WorldStats getStats() const override;
    [[nodiscard]] std::uint64_t computeStateHash() const override;

    [[nodiscard]] std::optional<BodyId> createBody(const Body::Options& options) override;
    void destroyBody(BodyId id) override;
    [[nodiscard]] bool isBodyValid(BodyId id) const noexcept override;
    [[nodiscard]] std::size_t getBodySlot(BodyId id) const noexcept override;
    [[nodiscard]] Body::Type getBodyType(BodyId id) const override;
    void setBodyType(BodyId id, Body::Type value) override;
    [[nodiscard]] Body::MotionQuality getMotionQuality(BodyId id) const override;
    void setMotionQuality(BodyId id, Body::MotionQuality value) override;
    [[nodiscard]] Body::MotionLocks getMotionLocks(BodyId id) const override;
    void setMotionLocks(BodyId id, const Body::MotionLocks& value) override;
    [[nodiscard]] Body::Transform getBodyTransform(BodyId id) const override;
    void setBodyTransform(BodyId id, const Body::Transform& value) override;
    void moveBody(BodyId id, const Body::Transform& target, float seconds) override;
    void readBodyTransforms(std::span<const BodyId> ids, std::span<float> values) const override;
    void writeBodyTransforms(std::span<const BodyId> ids, std::span<const float> values) override;
    [[nodiscard]] math::Vec3 getBodyVector(BodyId id, Body::Vector which) const override;
    void setBodyVector(BodyId id, Body::Vector which, math::Vec3 value) override;
    [[nodiscard]] float getBodyValue(BodyId id, Body::Value which) const override;
    void setBodyValue(BodyId id, Body::Value which, float value) override;
    [[nodiscard]] bool getBodyFlag(BodyId id, Body::Flag which) const override;
    void setBodyFlag(BodyId id, Body::Flag which, bool value) override;
    [[nodiscard]] Body::CombineRule getCombineRule(BodyId id, Body::Material which) const override;
    void setCombineRule(BodyId id, Body::Material which, Body::CombineRule value) override;
    void resetMass(BodyId id) override;
    [[nodiscard]] math::Vec3 getVelocityAt(BodyId id, math::Vec3 point) const override;
    void pushBody(BodyId id, Body::Push which, math::Vec3 value, std::optional<math::Vec3> point) override;
    [[nodiscard]] math::Box getBodyBounds(BodyId id) const override;
    [[nodiscard]] std::shared_ptr<const physics3d::Shape> getBodyShape(BodyId id) const override;
    void setBodyShape(BodyId id, const std::shared_ptr<const physics3d::Shape>& value) override;
    [[nodiscard]] CollisionFilter getBodyFilter(BodyId id) const override;
    void setBodyFilter(BodyId id, const CollisionFilter& value) override;
    [[nodiscard]] std::size_t getChildCount(BodyId id) const override;
    std::size_t addChild(BodyId id, const physics3d::Shape::Child& child) override;
    void removeChild(BodyId id, std::size_t index) override;
    void setChildTransform(BodyId id, std::size_t index, const Body::Transform& value) override;
    void collectActiveBodies(std::vector<BodyId>& ids) const override;

    [[nodiscard]] JointId createJoint(Joint::Type type, BodyId first, std::optional<BodyId> second, const Joint::Options& options) override;
    void destroyJoint(JointId id) override;
    [[nodiscard]] bool isJointValid(JointId id) const noexcept override;
    [[nodiscard]] Joint::Type getJointType(JointId id) const override;
    [[nodiscard]] std::pair<BodyId, std::optional<BodyId>> getJointBodies(JointId id) const override;
    [[nodiscard]] bool isJointEnabled(JointId id) const override;
    void setJointEnabled(JointId id, bool value) override;
    [[nodiscard]] int getJointPriority(JointId id) const override;
    void setJointPriority(JointId id, int value) override;
    [[nodiscard]] Joint::Load getJointLoad(JointId id) const override;
    [[nodiscard]] float getJointValue(JointId id, Joint::Value which) const override;
    void setJointValue(JointId id, Joint::Value which, float value) override;
    void setJointLimits(JointId id, float lower, float upper) override;
    [[nodiscard]] Joint::Spring getLimitSpring(JointId id) const override;
    void setLimitSpring(JointId id, const Joint::Spring& value) override;
    [[nodiscard]] Joint::Motor getJointMotor(JointId id, Joint::MotorSlot slot) const override;
    void setJointMotor(JointId id, Joint::MotorSlot slot, const Joint::Motor& value) override;
    [[nodiscard]] math::Quat getTargetRotation(JointId id) const override;
    void setTargetRotation(JointId id, const math::Quat& value) override;
    [[nodiscard]] math::Vec3 getTargetAngularVelocity(JointId id) const override;
    void setTargetAngularVelocity(JointId id, math::Vec3 value) override;
    [[nodiscard]] Joint::AxisSettings getJointAxis(JointId id, Joint::Axis axis) const override;
    void setJointAxis(JointId id, Joint::Axis axis, const Joint::AxisSettings& value) override;

    // The physics system, which the bodies, shapes and constraints of the world live in.
    [[nodiscard]] JPH::PhysicsSystem& getSystem() noexcept {
        return system;
    }

    // The layers of the bodies, which the filters of queries read.
    [[nodiscard]] Layers& getLayers() noexcept {
        return layers;
    }

  private:
    // What a body keeps beside its body of Jolt: the shape it took, its own copy of a mutable compound, and the mass, inertia and center of mass that were set.
    struct Record {
        std::shared_ptr<const physics3d::Shape> shape;
        JPH::Ref<JPH::MutableCompoundShape> compound;
        std::optional<float> mass;
        std::optional<math::Vec3> inertia;
        std::optional<math::Vec3> centerOfMass;
    };

    static constexpr std::size_t kMegabyte = std::size_t{1} << 20;

    [[nodiscard]] JPH::Body& getBody(BodyId id) const;
    [[nodiscard]] Record& getRecord(BodyId id);
    [[nodiscard]] const Record& getRecord(BodyId id) const;

    // The shape the body of Jolt takes: the shape of the record, wrapped to move its center of mass when one was set.
    [[nodiscard]] JPH::RefConst<JPH::Shape> getJoltShape(const Record& record) const;

    // The mass properties of the shape with the mass and inertia of the record.
    [[nodiscard]] static JPH::MassProperties getMass(const Record& record, const JPH::Shape& shape);

    void wake(BodyId id);

    // Gives a dynamic body the mass of its record and shape, with its locks, and drops the velocities along the locked axes.
    void applyMass(BodyId id);

    // Gives the body the shape of its record, keeping where its shape is in the world, and its mass.
    void applyShape(BodyId id);

    // A mutable compound becomes the own copy of the body, and every other shape is shared.
    void takeShape(Record& record, const std::shared_ptr<const physics3d::Shape>& shape) const;

    // Tells Jolt that the own compound of a body changed, and remakes the shape of the engine that reports it.
    void changeCompound(BodyId id, JPH::Vec3 previousCenter);

    [[nodiscard]] static JPH::EMotionType toJolt(Body::Type value) noexcept;
    [[nodiscard]] static JPH::EAllowedDOFs toJolt(const Body::MotionLocks& value) noexcept;
    [[nodiscard]] static Body::MotionLocks fromJolt(JPH::EAllowedDOFs value) noexcept;
    [[nodiscard]] JPH::EActivation activationOf(BodyId id) const;

    // Gives the body the layer of its filter in the part of the broad phase that its type and sensor take.
    void applyLayer(BodyId id, const CollisionFilter& filter);
    [[nodiscard]] static Layers::Part partOf(Body::Type type, bool sensor) noexcept;

    Layers layers;
    JPH::PhysicsSystem system;
    StepAllocator allocator;
    std::shared_ptr<StepJobs> jobs;
    std::vector<Record> records;
    ContactFilter contacts;
    Joints joints{system, contacts};
    int collisionSteps = 1;
    float subStep = 0.0F;
    int stepThreads = 0;
    float stepMilliseconds = 0.0F;
};

} // namespace haylen::physics3d::jolt
