#include "3d/physics/jolt/Joints.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/GearConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraintPathHermite.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "3d/physics/jolt/Converter.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics3d::jolt {

JPH::SpringSettings Joints::toJolt(const Joint::Spring& value) noexcept {
    if (value.mode == Joint::SpringMode::Stiffness) {
        return {JPH::ESpringMode::StiffnessAndDamping, value.stiffness, value.damping};
    }

    return {JPH::ESpringMode::FrequencyAndDamping, value.frequency, value.damping};
}

JPH::MotorSettings Joints::toJolt(const Joint::Motor& value) noexcept {
    JPH::MotorSettings settings;
    settings.mSpringSettings = toJolt(value.spring);
    settings.SetForceLimit(value.maxForce);
    settings.SetTorqueLimit(value.maxTorque);
    return settings;
}

JPH::EMotorState Joints::toJolt(Joint::MotorState value) noexcept {
    switch (value) {
    case Joint::MotorState::Velocity:
        return JPH::EMotorState::Velocity;
    case Joint::MotorState::Position:
        return JPH::EMotorState::Position;
    case Joint::MotorState::Off:
        break;
    }

    return JPH::EMotorState::Off;
}

JPH::EPathRotationConstraintType Joints::toJolt(Joint::PathRotation value) noexcept {
    switch (value) {
    case Joint::PathRotation::AroundTangent:
        return JPH::EPathRotationConstraintType::ConstrainAroundTangent;
    case Joint::PathRotation::AroundNormal:
        return JPH::EPathRotationConstraintType::ConstrainAroundNormal;
    case Joint::PathRotation::AroundBinormal:
        return JPH::EPathRotationConstraintType::ConstrainAroundBinormal;
    case Joint::PathRotation::WithPath:
        return JPH::EPathRotationConstraintType::ConstrainToPath;
    case Joint::PathRotation::Fixed:
        return JPH::EPathRotationConstraintType::FullyConstrained;
    case Joint::PathRotation::Free:
        break;
    }

    return JPH::EPathRotationConstraintType::Free;
}

const Joints::Record& Joints::getRecord(JointId id) const {
    return records[id & kIndexMask];
}

Joints::Record& Joints::getRecord(JointId id) {
    return records[id & kIndexMask];
}

JPH::Body& Joints::getBody(std::optional<BodyId> id) const {
    if (!id) {
        return JPH::Body::sFixedToWorld;
    }

    return *system.GetBodyLockInterfaceNoLock().TryGetBody(JPH::BodyID(*id));
}

bool Joints::isValid(JointId id) const noexcept {
    const std::size_t index = id & kIndexMask;
    return index < records.size() && records[index].constraint != nullptr && records[index].generation == static_cast<std::uint8_t>(id >> kIndexBits);
}

// The spline is sampled into a path of Hermite curves in the space of the first body, with tangents from the samples around each one and normals that turn as little as the path does from one sample to the next.
JPH::Ref<JPH::PathConstraintPath> Joints::makePath(const math::Spline3D& spline, const JPH::Body& first) {
    const std::size_t count = std::clamp<std::size_t>(spline.getSegmentCount() * kSamplesPerSegment, kMinPathSamples, kMaxPathSamples);
    const bool closed = spline.isClosed();
    std::vector<JPH::Vec3> points;
    points.reserve(count);
    const JPH::RMat44 toBody = first.GetWorldTransform().InversedRotationTranslation();
    for (std::size_t index = 0; index < count; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(closed ? count : count - 1);
        points.push_back(toBody * Converter::toJolt(spline.getPoint(t)));
    }

    JPH::Ref<JPH::PathConstraintPathHermite> path = new JPH::PathConstraintPathHermite();
    path->SetIsLooping(closed);
    JPH::Vec3 normal = JPH::Vec3::sZero();
    for (std::size_t index = 0; index < count; ++index) {
        const JPH::Vec3 before = points[index == 0 ? (closed ? count - 1 : 0) : index - 1];
        const JPH::Vec3 after = points[index + 1 == count ? (closed ? 0 : count - 1) : index + 1];
        const float span = closed || (index > 0 && index + 1 < count) ? 0.5F : 1.0F;
        const JPH::Vec3 tangent = (after - before) * span;
        const JPH::Vec3 direction = tangent.NormalizedOr(JPH::Vec3::sAxisX());
        normal = index == 0 ? direction.GetNormalizedPerpendicular() : (normal - direction * normal.Dot(direction)).NormalizedOr(direction.GetNormalizedPerpendicular());
        path->AddPoint(points[index], tangent, normal);
    }

    return path.GetPtr();
}

Joints::JointId Joints::create(Joint::Type type, BodyId firstId, std::optional<BodyId> secondId, const Joint::Options& options) {
    // Jolt measures the second body against the first, so a joint to the world takes the world as its first body and moves the body of the app against it.
    JPH::Body& first = secondId ? getBody(firstId) : JPH::Body::sFixedToWorld;
    JPH::Body& second = secondId ? getBody(secondId) : getBody(firstId);
    const JPH::RVec3 point = Converter::toJolt(options.point);
    const JPH::RVec3 secondPoint = Converter::toJolt(options.secondPoint.value_or(options.point));
    const JPH::Vec3 axis = Converter::toJolt(options.axis).Normalized();
    const JPH::Vec3 normal = Converter::toJolt(options.normal).Normalized();
    JPH::Ref<JPH::TwoBodyConstraint> constraint;

    // clang-format off
    switch (type) {
    case Joint::Type::Fixed: {
        JPH::FixedConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Point: {
        JPH::PointConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = secondPoint;
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Distance: {
        JPH::DistanceConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = secondPoint;
        settings.mMinDistance = options.lower.value_or(-1.0F);
        settings.mMaxDistance = options.upper.value_or(-1.0F);
        settings.mLimitsSpringSettings = toJolt(options.limitSpring);
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Hinge: {
        JPH::HingeConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = secondPoint;
        settings.mHingeAxis1 = settings.mHingeAxis2 = axis;
        settings.mNormalAxis1 = settings.mNormalAxis2 = normal;
        settings.mLimitsMin = options.lower.value_or(-JPH::JPH_PI);
        settings.mLimitsMax = options.upper.value_or(JPH::JPH_PI);
        settings.mLimitsSpringSettings = toJolt(options.limitSpring);
        settings.mMaxFrictionTorque = options.friction;
        settings.mMotorSettings = toJolt(options.motor);
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Slider: {
        JPH::SliderConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = secondPoint;
        settings.mSliderAxis1 = settings.mSliderAxis2 = axis;
        settings.mNormalAxis1 = settings.mNormalAxis2 = normal;
        settings.mLimitsMin = options.lower.value_or(-FLT_MAX);
        settings.mLimitsMax = options.upper.value_or(FLT_MAX);
        settings.mLimitsSpringSettings = toJolt(options.limitSpring);
        settings.mMaxFrictionForce = options.friction;
        settings.mMotorSettings = toJolt(options.motor);
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Cone: {
        JPH::ConeConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = secondPoint;
        settings.mTwistAxis1 = settings.mTwistAxis2 = axis;
        settings.mHalfConeAngle = options.halfConeAngle;
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::SwingTwist: {
        JPH::SwingTwistConstraintSettings settings;
        settings.mPosition1 = point;
        settings.mPosition2 = secondPoint;
        settings.mTwistAxis1 = settings.mTwistAxis2 = axis;
        settings.mPlaneAxis1 = settings.mPlaneAxis2 = normal;
        settings.mNormalHalfConeAngle = options.halfConeAngle;
        settings.mPlaneHalfConeAngle = options.planeHalfConeAngle;
        settings.mTwistMinAngle = options.lower.value_or(-JPH::JPH_PI);
        settings.mTwistMaxAngle = options.upper.value_or(JPH::JPH_PI);
        settings.mMaxFrictionTorque = options.friction;
        settings.mTwistMotorSettings = toJolt(options.motor);
        settings.mSwingMotorSettings = toJolt(options.swingMotor);
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::SixDof: {
        JPH::SixDOFConstraintSettings settings;
        settings.mPosition1 = point;
        settings.mPosition2 = secondPoint;
        settings.mAxisX1 = settings.mAxisX2 = axis;
        settings.mAxisY1 = settings.mAxisY2 = normal;
        for (std::size_t index = 0; index < options.axes.size(); ++index) {
            const auto joltAxis = static_cast<JPH::SixDOFConstraintSettings::EAxis>(index);
            const Joint::AxisSettings& each = options.axes[index];
            if (each.mode == Joint::AxisMode::Fixed) {
                settings.MakeFixedAxis(joltAxis);
            } else if (each.mode == Joint::AxisMode::Limited) {
                settings.SetLimitedAxis(joltAxis, each.lower, each.upper);
            }

            if (index < static_cast<std::size_t>(JPH::SixDOFConstraintSettings::EAxis::NumTranslation)) {
                settings.mLimitsSpringSettings[index] = toJolt(each.limitSpring);
            }

            settings.mMaxFriction[index] = each.friction;
            settings.mMotorSettings[index] = toJolt(each.motor);
        }

        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Path: {
        JPH::PathConstraintSettings settings;
        settings.mPath = makePath(*options.path, first);
        settings.mPathFraction = options.pathFraction * settings.mPath->GetPathMaxFraction();
        settings.mRotationConstraintType = toJolt(options.pathRotation);
        settings.mMaxFrictionForce = options.friction;
        settings.mPositionMotorSettings = toJolt(options.motor);
        constraint = settings.Create(first, second);
        break;
    }
    case Joint::Type::Gear: {
        JPH::GearConstraintSettings settings;
        settings.mHingeAxis1 = axis;
        settings.mHingeAxis2 = Converter::toJolt(options.secondAxis).Normalized();
        settings.mRatio = options.ratio;
        JPH::Ref<JPH::GearConstraint> gear = dynamic_cast<JPH::GearConstraint*>(settings.Create(first, second));
        gear->SetConstraints(getRecord(options.joints[0].getId()).constraint, getRecord(options.joints[1].getId()).constraint);
        constraint = gear.GetPtr();
        break;
    }
    case Joint::Type::RackAndPinion: {
        JPH::RackAndPinionConstraintSettings settings;
        settings.mHingeAxis = axis;
        settings.mSliderAxis = Converter::toJolt(options.secondAxis).Normalized();
        settings.mRatio = options.ratio;
        JPH::Ref<JPH::RackAndPinionConstraint> rack = dynamic_cast<JPH::RackAndPinionConstraint*>(settings.Create(first, second));
        rack->SetConstraints(getRecord(options.joints[0].getId()).constraint, getRecord(options.joints[1].getId()).constraint);
        constraint = rack.GetPtr();
        break;
    }
    case Joint::Type::Pulley: {
        JPH::PulleyConstraintSettings settings;
        settings.mBodyPoint1 = point;
        settings.mBodyPoint2 = secondPoint;
        settings.mFixedPoint1 = Converter::toJolt(options.fixedPoint);
        settings.mFixedPoint2 = Converter::toJolt(options.secondFixedPoint);
        settings.mRatio = options.ratio;
        settings.mMinLength = options.lower.value_or(0.0F);
        settings.mMaxLength = options.upper.value_or(-1.0F);
        constraint = settings.Create(first, second);
        break;
    }
    }
    // clang-format on

    constraint->SetConstraintPriority(static_cast<JPH::uint32>(options.priority));
    constraint->SetNumVelocityStepsOverride(static_cast<JPH::uint>(options.velocitySteps));
    constraint->SetNumPositionStepsOverride(static_cast<JPH::uint>(options.positionSteps));
    constraint->SetEnabled(options.enabled);

    std::size_t index = records.size();
    if (!freeSlots.empty()) {
        index = freeSlots.back();
        freeSlots.pop_back();
    } else {
        records.emplace_back();
    }

    Record& record = records[index];
    record.constraint = constraint;
    record.type = type;
    record.first = firstId;
    record.second = secondId;
    record.collideConnected = options.collideConnected;
    record.main = options.motor;
    record.swing = options.swingMotor;
    record.axes = options.axes;
    record.limitSpring = options.limitSpring;
    record.targetRotation = options.targetRotation;
    record.targetAngularVelocity = options.targetAngularVelocity;
    record.ratio = options.ratio;
    const JointId id = (static_cast<JointId>(record.generation) << kIndexBits) | static_cast<JointId>(index);
    for (const Joint::MotorSlot slot : {Joint::MotorSlot::Main, Joint::MotorSlot::Swing, Joint::MotorSlot::X, Joint::MotorSlot::Y, Joint::MotorSlot::Z, Joint::MotorSlot::RotationX, Joint::MotorSlot::RotationY, Joint::MotorSlot::RotationZ}) {
        applyMotor(record, slot);
    }

    system.AddConstraint(constraint);
    if (secondId && !options.collideConnected) {
        contacts.addPair(firstId, *secondId);
    }

    return id;
}

void Joints::destroy(JointId id) {
    Record& record = getRecord(id);
    system.RemoveConstraint(record.constraint);
    if (record.second && !record.collideConnected) {
        contacts.removePair(record.first, *record.second);
    }

    const auto generation = static_cast<std::uint8_t>(record.generation + 1);
    record = {};
    record.generation = generation;
    freeSlots.push_back(id & kIndexMask);
}

void Joints::destroyJointsOf(BodyId body) {
    for (std::size_t index = 0; index < records.size(); ++index) {
        const Record& record = records[index];
        if (record.constraint != nullptr && (record.first == body || record.second == body)) {
            destroy((static_cast<JointId>(record.generation) << kIndexBits) | static_cast<JointId>(index));
        }
    }
}

Joint::Type Joints::getType(JointId id) const {
    return getRecord(id).type;
}

std::pair<Joints::BodyId, std::optional<Joints::BodyId>> Joints::getBodies(JointId id) const {
    const Record& record = getRecord(id);
    return {record.first, record.second};
}

bool Joints::isEnabled(JointId id) const {
    return getRecord(id).constraint->GetEnabled();
}

void Joints::setEnabled(JointId id, bool value) {
    getRecord(id).constraint->SetEnabled(value);
}

int Joints::getPriority(JointId id) const {
    return static_cast<int>(getRecord(id).constraint->GetConstraintPriority());
}

void Joints::setPriority(JointId id, int value) {
    getRecord(id).constraint->SetConstraintPriority(static_cast<JPH::uint32>(value));
}

// The solver hands each part of a constraint an impulse in the last collision step, so the force is the size of the impulses that hold the bodies in place over the time of that step, and the torque the size of those that hold their rotation.
Joint::Load Joints::getLoad(JointId id, float subStep) const {
    const Record& record = getRecord(id);
    if (subStep <= 0.0F) {
        return {};
    }

    const JPH::TwoBodyConstraint& constraint = *record.constraint;
    float force = 0.0F;
    float torque = 0.0F;
    switch (record.type) {
    case Joint::Type::Fixed: {
        const auto& fixed = dynamic_cast<const JPH::FixedConstraint&>(constraint);
        force = fixed.GetTotalLambdaPosition().Length();
        torque = fixed.GetTotalLambdaRotation().Length();
        break;
    }
    case Joint::Type::Point:
        force = dynamic_cast<const JPH::PointConstraint&>(constraint).GetTotalLambdaPosition().Length();
        break;
    case Joint::Type::Distance:
        force = std::abs(dynamic_cast<const JPH::DistanceConstraint&>(constraint).GetTotalLambdaPosition());
        break;
    case Joint::Type::Hinge: {
        const auto& hinge = dynamic_cast<const JPH::HingeConstraint&>(constraint);
        const JPH::Vector<2> rotation = hinge.GetTotalLambdaRotation();
        const float around = hinge.GetTotalLambdaRotationLimits() + hinge.GetTotalLambdaMotor();
        force = hinge.GetTotalLambdaPosition().Length();
        torque = std::sqrt(rotation[0] * rotation[0] + rotation[1] * rotation[1] + around * around);
        break;
    }
    case Joint::Type::Slider: {
        const auto& slider = dynamic_cast<const JPH::SliderConstraint&>(constraint);
        const JPH::Vector<2> position = slider.GetTotalLambdaPosition();
        const float along = slider.GetTotalLambdaPositionLimits() + slider.GetTotalLambdaMotor();
        force = std::sqrt(position[0] * position[0] + position[1] * position[1] + along * along);
        torque = slider.GetTotalLambdaRotation().Length();
        break;
    }
    case Joint::Type::Cone: {
        const auto& cone = dynamic_cast<const JPH::ConeConstraint&>(constraint);
        force = cone.GetTotalLambdaPosition().Length();
        torque = std::abs(cone.GetTotalLambdaRotation());
        break;
    }
    case Joint::Type::SwingTwist: {
        const auto& swing = dynamic_cast<const JPH::SwingTwistConstraint&>(constraint);
        const JPH::Vec3 turn(swing.GetTotalLambdaTwist(), swing.GetTotalLambdaSwingY(), swing.GetTotalLambdaSwingZ());
        force = swing.GetTotalLambdaPosition().Length();
        torque = (turn + swing.GetTotalLambdaMotor()).Length();
        break;
    }
    case Joint::Type::SixDof: {
        const auto& six = dynamic_cast<const JPH::SixDOFConstraint&>(constraint);
        force = (six.GetTotalLambdaPosition() + six.GetTotalLambdaMotorTranslation()).Length();
        torque = (six.GetTotalLambdaRotation() + six.GetTotalLambdaMotorRotation()).Length();
        break;
    }
    case Joint::Type::Path: {
        const auto& path = dynamic_cast<const JPH::PathConstraint&>(constraint);
        const JPH::Vector<2> position = path.GetTotalLambdaPosition();
        const float along = path.GetTotalLambdaPositionLimits() + path.GetTotalLambdaMotor();
        const JPH::Vector<2> hinge = path.GetTotalLambdaRotationHinge();
        force = std::sqrt(position[0] * position[0] + position[1] * position[1] + along * along);
        torque = std::sqrt(hinge[0] * hinge[0] + hinge[1] * hinge[1]) + path.GetTotalLambdaRotation().Length();
        break;
    }
    case Joint::Type::Gear:
        torque = std::abs(dynamic_cast<const JPH::GearConstraint&>(constraint).GetTotalLambda());
        break;
    case Joint::Type::RackAndPinion:
        torque = std::abs(dynamic_cast<const JPH::RackAndPinionConstraint&>(constraint).GetTotalLambda());
        break;
    case Joint::Type::Pulley:
        force = std::abs(dynamic_cast<const JPH::PulleyConstraint&>(constraint).GetTotalLambdaPosition());
        break;
    }

    return {.force = force / subStep, .torque = torque / subStep};
}

float Joints::getLimit(const Record& record, bool lower) {
    const JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (record.type) {
    case Joint::Type::Hinge: {
        const auto& hinge = dynamic_cast<const JPH::HingeConstraint&>(constraint);
        return lower ? hinge.GetLimitsMin() : hinge.GetLimitsMax();
    }
    case Joint::Type::Slider: {
        const auto& slider = dynamic_cast<const JPH::SliderConstraint&>(constraint);
        return lower ? slider.GetLimitsMin() : slider.GetLimitsMax();
    }
    case Joint::Type::Distance: {
        const auto& distance = dynamic_cast<const JPH::DistanceConstraint&>(constraint);
        return lower ? distance.GetMinDistance() : distance.GetMaxDistance();
    }
    case Joint::Type::SwingTwist: {
        const auto& swing = dynamic_cast<const JPH::SwingTwistConstraint&>(constraint);
        return lower ? swing.GetTwistMinAngle() : swing.GetTwistMaxAngle();
    }
    default:
        break;
    }

    const auto& pulley = dynamic_cast<const JPH::PulleyConstraint&>(constraint);
    return lower ? pulley.GetMinLength() : pulley.GetMaxLength();
}

float Joints::getFriction(const Record& record) {
    const JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (record.type) {
    case Joint::Type::Hinge:
        return dynamic_cast<const JPH::HingeConstraint&>(constraint).GetMaxFrictionTorque();
    case Joint::Type::Slider:
        return dynamic_cast<const JPH::SliderConstraint&>(constraint).GetMaxFrictionForce();
    case Joint::Type::SwingTwist:
        return dynamic_cast<const JPH::SwingTwistConstraint&>(constraint).GetMaxFrictionTorque();
    default:
        break;
    }

    return dynamic_cast<const JPH::PathConstraint&>(constraint).GetMaxFrictionForce();
}

float Joints::getValue(JointId id, Joint::Value which) const {
    const Record& record = getRecord(id);
    const JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (which) {
    case Joint::Value::Lower:
        return getLimit(record, true);
    case Joint::Value::Upper:
        return getLimit(record, false);
    case Joint::Value::Friction:
        return getFriction(record);
    case Joint::Value::HalfConeAngle:
        if (record.type == Joint::Type::Cone) {
            return std::acos(std::clamp(dynamic_cast<const JPH::ConeConstraint&>(constraint).GetCosHalfConeAngle(), -1.0F, 1.0F));
        }

        return dynamic_cast<const JPH::SwingTwistConstraint&>(constraint).GetNormalHalfConeAngle();
    case Joint::Value::PlaneHalfConeAngle:
        return dynamic_cast<const JPH::SwingTwistConstraint&>(constraint).GetPlaneHalfConeAngle();
    case Joint::Value::Ratio:
        return record.ratio;
    case Joint::Value::Angle:
        return dynamic_cast<const JPH::HingeConstraint&>(constraint).GetCurrentAngle();
    case Joint::Value::Position:
        return dynamic_cast<const JPH::SliderConstraint&>(constraint).GetCurrentPosition();
    case Joint::Value::Length: {
        const JPH::RVec3 first = constraint.GetBody1()->GetCenterOfMassTransform() * constraint.GetConstraintToBody1Matrix().GetTranslation();
        const JPH::RVec3 second = constraint.GetBody2()->GetCenterOfMassTransform() * constraint.GetConstraintToBody2Matrix().GetTranslation();
        return JPH::Vec3(second - first).Length();
    }
    case Joint::Value::Fraction:
        break;
    }

    const auto& path = dynamic_cast<const JPH::PathConstraint&>(constraint);
    return path.GetPathFraction() / path.GetPath()->GetPathMaxFraction();
}

void Joints::setValue(JointId id, Joint::Value which, float value) {
    Record& record = getRecord(id);
    JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (which) {
    case Joint::Value::Friction:
        if (record.type == Joint::Type::Hinge) {
            dynamic_cast<JPH::HingeConstraint&>(constraint).SetMaxFrictionTorque(value);
        } else if (record.type == Joint::Type::Slider) {
            dynamic_cast<JPH::SliderConstraint&>(constraint).SetMaxFrictionForce(value);
        } else if (record.type == Joint::Type::SwingTwist) {
            dynamic_cast<JPH::SwingTwistConstraint&>(constraint).SetMaxFrictionTorque(value);
        } else {
            dynamic_cast<JPH::PathConstraint&>(constraint).SetMaxFrictionForce(value);
        }

        break;
    case Joint::Value::HalfConeAngle:
        if (record.type == Joint::Type::Cone) {
            dynamic_cast<JPH::ConeConstraint&>(constraint).SetHalfConeAngle(value);
        } else {
            dynamic_cast<JPH::SwingTwistConstraint&>(constraint).SetNormalHalfConeAngle(value);
        }

        break;
    case Joint::Value::PlaneHalfConeAngle:
        dynamic_cast<JPH::SwingTwistConstraint&>(constraint).SetPlaneHalfConeAngle(value);
        break;
    default:
        return;
    }

    system.GetBodyInterfaceNoLock().ActivateConstraint(&constraint);
}

void Joints::setLimits(JointId id, float lower, float upper) {
    Record& record = getRecord(id);
    JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (record.type) {
    case Joint::Type::Hinge:
        dynamic_cast<JPH::HingeConstraint&>(constraint).SetLimits(lower, upper);
        break;
    case Joint::Type::Slider:
        dynamic_cast<JPH::SliderConstraint&>(constraint).SetLimits(lower, upper);
        break;
    case Joint::Type::Distance:
        dynamic_cast<JPH::DistanceConstraint&>(constraint).SetDistance(lower, upper);
        break;
    case Joint::Type::SwingTwist: {
        auto& swing = dynamic_cast<JPH::SwingTwistConstraint&>(constraint);
        swing.SetTwistMinAngle(lower);
        swing.SetTwistMaxAngle(upper);
        break;
    }
    default:
        dynamic_cast<JPH::PulleyConstraint&>(constraint).SetLength(lower, upper);
        break;
    }

    system.GetBodyInterfaceNoLock().ActivateConstraint(&constraint);
}

Joint::Spring Joints::getLimitSpring(JointId id) const {
    return getRecord(id).limitSpring;
}

void Joints::setLimitSpring(JointId id, const Joint::Spring& value) {
    Record& record = getRecord(id);
    JPH::TwoBodyConstraint& constraint = *record.constraint;
    record.limitSpring = value;
    if (record.type == Joint::Type::Hinge) {
        dynamic_cast<JPH::HingeConstraint&>(constraint).SetLimitsSpringSettings(toJolt(value));
    } else if (record.type == Joint::Type::Slider) {
        dynamic_cast<JPH::SliderConstraint&>(constraint).SetLimitsSpringSettings(toJolt(value));
    } else {
        dynamic_cast<JPH::DistanceConstraint&>(constraint).SetLimitsSpringSettings(toJolt(value));
    }

    system.GetBodyInterfaceNoLock().ActivateConstraint(&constraint);
}

Joint::Motor Joints::getMotor(JointId id, Joint::MotorSlot slot) const {
    const Record& record = getRecord(id);
    switch (slot) {
    case Joint::MotorSlot::Main:
        return record.main;
    case Joint::MotorSlot::Swing:
        return record.swing;
    default:
        return record.axes[static_cast<std::size_t>(slot) - static_cast<std::size_t>(Joint::MotorSlot::X)].motor;
    }
}

void Joints::setMotor(JointId id, Joint::MotorSlot slot, const Joint::Motor& value) {
    Record& record = getRecord(id);
    switch (slot) {
    case Joint::MotorSlot::Main:
        record.main = value;
        break;
    case Joint::MotorSlot::Swing:
        record.swing = value;
        break;
    default:
        record.axes[static_cast<std::size_t>(slot) - static_cast<std::size_t>(Joint::MotorSlot::X)].motor = value;
        break;
    }

    applyMotor(record, slot);
    system.GetBodyInterfaceNoLock().ActivateConstraint(record.constraint);
}

// A motor of position on a path drives toward its share of the path, and the targets of six degrees of freedom gather the targets of the motors of its axes.
void Joints::applyMotor(Record& record, Joint::MotorSlot slot) {
    JPH::TwoBodyConstraint& constraint = *record.constraint;
    switch (record.type) {
    case Joint::Type::Hinge: {
        if (slot != Joint::MotorSlot::Main) {
            return;
        }

        auto& hinge = dynamic_cast<JPH::HingeConstraint&>(constraint);
        hinge.GetMotorSettings() = toJolt(record.main);
        hinge.SetTargetAngularVelocity(record.main.targetVelocity);
        hinge.SetTargetAngle(record.main.targetPosition);
        hinge.SetMotorState(toJolt(record.main.state));
        return;
    }
    case Joint::Type::Slider: {
        if (slot != Joint::MotorSlot::Main) {
            return;
        }

        auto& slider = dynamic_cast<JPH::SliderConstraint&>(constraint);
        slider.GetMotorSettings() = toJolt(record.main);
        slider.SetTargetVelocity(record.main.targetVelocity);
        slider.SetTargetPosition(record.main.targetPosition);
        slider.SetMotorState(toJolt(record.main.state));
        return;
    }
    case Joint::Type::Path: {
        if (slot != Joint::MotorSlot::Main) {
            return;
        }

        auto& path = dynamic_cast<JPH::PathConstraint&>(constraint);
        path.GetPositionMotorSettings() = toJolt(record.main);
        path.SetTargetVelocity(record.main.targetVelocity);
        path.SetTargetPathFraction(record.main.targetPosition * path.GetPath()->GetPathMaxFraction());
        path.SetPositionMotorState(toJolt(record.main.state));
        return;
    }
    case Joint::Type::SwingTwist: {
        auto& swing = dynamic_cast<JPH::SwingTwistConstraint&>(constraint);
        swing.GetTwistMotorSettings() = toJolt(record.main);
        swing.GetSwingMotorSettings() = toJolt(record.swing);
        swing.SetTargetOrientationCS(Converter::toJolt(record.targetRotation));
        swing.SetTargetAngularVelocityCS(Converter::toJolt(record.targetAngularVelocity));
        swing.SetTwistMotorState(toJolt(record.main.state));
        swing.SetSwingMotorState(toJolt(record.swing.state));
        return;
    }
    case Joint::Type::SixDof: {
        if (slot == Joint::MotorSlot::Main || slot == Joint::MotorSlot::Swing) {
            return;
        }

        auto& six = dynamic_cast<JPH::SixDOFConstraint&>(constraint);
        const std::size_t index = static_cast<std::size_t>(slot) - static_cast<std::size_t>(Joint::MotorSlot::X);
        const auto axis = static_cast<JPH::SixDOFConstraintSettings::EAxis>(index);
        six.GetMotorSettings(axis) = toJolt(record.axes[index].motor);
        six.SetTargetVelocityCS(gatherTargets(record, 0, true));
        six.SetTargetPositionCS(gatherTargets(record, 0, false));
        six.SetTargetAngularVelocityCS(gatherTargets(record, 3, true));
        six.SetTargetOrientationCS(Converter::toJolt(record.targetRotation));
        six.SetMotorState(axis, toJolt(record.axes[index].motor.state));
        return;
    }
    default:
        return;
    }
}

JPH::Vec3 Joints::gatherTargets(const Record& record, std::size_t first, bool velocity) noexcept {
    std::array<float, 3> targets{};
    for (std::size_t each = 0; each < targets.size(); ++each) {
        const Joint::Motor& motor = record.axes[first + each].motor;
        targets[each] = velocity ? motor.targetVelocity : motor.targetPosition;
    }

    return {targets[0], targets[1], targets[2]};
}

// A fixed axis has a lower limit above its upper one, as Jolt reads it, and a free axis the widest limits.
float Joints::limitOf(const Joint::AxisSettings& settings, bool lower) noexcept {
    switch (settings.mode) {
    case Joint::AxisMode::Fixed:
        return lower ? FLT_MAX : -FLT_MAX;
    case Joint::AxisMode::Limited:
        return lower ? settings.lower : settings.upper;
    case Joint::AxisMode::Free:
        break;
    }

    return lower ? -FLT_MAX : FLT_MAX;
}

JPH::Vec3 Joints::gatherLimits(const Record& record, std::size_t first, bool lower) noexcept {
    return {limitOf(record.axes[first], lower), limitOf(record.axes[first + 1], lower), limitOf(record.axes[first + 2], lower)};
}

math::Quat Joints::getTargetRotation(JointId id) const {
    return getRecord(id).targetRotation;
}

void Joints::setTargetRotation(JointId id, const math::Quat& value) {
    Record& record = getRecord(id);
    record.targetRotation = value;
    if (record.type == Joint::Type::SwingTwist) {
        dynamic_cast<JPH::SwingTwistConstraint&>(*record.constraint).SetTargetOrientationCS(Converter::toJolt(value));
    } else {
        dynamic_cast<JPH::SixDOFConstraint&>(*record.constraint).SetTargetOrientationCS(Converter::toJolt(value));
    }

    system.GetBodyInterfaceNoLock().ActivateConstraint(record.constraint);
}

math::Vec3 Joints::getTargetAngularVelocity(JointId id) const {
    return getRecord(id).targetAngularVelocity;
}

void Joints::setTargetAngularVelocity(JointId id, math::Vec3 value) {
    Record& record = getRecord(id);
    record.targetAngularVelocity = value;
    dynamic_cast<JPH::SwingTwistConstraint&>(*record.constraint).SetTargetAngularVelocityCS(Converter::toJolt(value));
    system.GetBodyInterfaceNoLock().ActivateConstraint(record.constraint);
}

Joint::AxisSettings Joints::getAxis(JointId id, Joint::Axis axis) const {
    return getRecord(id).axes[static_cast<std::size_t>(axis)];
}

// Jolt holds the limits of the axes that move along and of the axes that turn as three of each, so a change of one axis sets the three of its kind again.
void Joints::setAxis(JointId id, Joint::Axis axis, const Joint::AxisSettings& value) {
    Record& record = getRecord(id);
    const auto index = static_cast<std::size_t>(axis);
    record.axes[index] = value;
    auto& six = dynamic_cast<JPH::SixDOFConstraint&>(*record.constraint);
    six.SetTranslationLimits(gatherLimits(record, 0, true), gatherLimits(record, 0, false));
    six.SetRotationLimits(gatherLimits(record, 3, true), gatherLimits(record, 3, false));
    const auto joltAxis = static_cast<JPH::SixDOFConstraintSettings::EAxis>(index);
    if (index < static_cast<std::size_t>(JPH::SixDOFConstraintSettings::EAxis::NumTranslation)) {
        six.SetLimitsSpringSettings(joltAxis, toJolt(value.limitSpring));
    }

    six.SetMaxFriction(joltAxis, value.friction);
    applyMotor(record, static_cast<Joint::MotorSlot>(static_cast<std::size_t>(Joint::MotorSlot::X) + index));
    system.GetBodyInterfaceNoLock().ActivateConstraint(record.constraint);
}

} // namespace haylen::physics3d::jolt
