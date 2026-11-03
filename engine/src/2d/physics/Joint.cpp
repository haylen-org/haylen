#include "haylen/2d/physics/Joint.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Joint::Type>, 8> Joint::kTypeNames{{{"distance", Type::Distance}, {"revolute", Type::Revolute}, {"prismatic", Type::Prismatic}, {"weld", Type::Weld}, {"wheel", Type::Wheel}, {"mouse", Type::Mouse}, {"motor", Type::Motor}, {"filter", Type::Filter}}};

std::optional<Joint::Type> Joint::typeFromName(std::string_view name) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (text == name) {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view Joint::typeName(Type value) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (type == value) {
            return text;
        }
    }
    return kTypeNames.front().first;
}

Joint::Joint(World* owner, std::uint64_t handle) noexcept : world(owner), worldHandle(owner != nullptr ? owner->handle : 0), id(handle) {}

std::uint64_t Joint::checkedId() const {
    if (world != nullptr && !b2World_IsValid(b2LoadWorldId(worldHandle))) {
        throw std::logic_error("The physics world of the joint was destroyed.");
    }
    if (!isValid()) {
        throw std::logic_error("The physics joint was destroyed.");
    }
    return id;
}

std::uint64_t Joint::checkedId(std::initializer_list<Type> types, const char* message) const {
    const std::uint64_t checked = checkedId();
    if (std::ranges::find(types, getType()) == types.end()) {
        throw std::logic_error(message);
    }
    return checked;
}

float Joint::getScale() const noexcept {
    return world->getPixelsPerMeter();
}

bool Joint::isValid() const noexcept {
    return world != nullptr && b2World_IsValid(b2LoadWorldId(worldHandle)) && b2Joint_IsValid(b2LoadJointId(id));
}

void Joint::destroy() {
    if (isValid()) {
        b2DestroyJoint(b2LoadJointId(id));
    }
}

Joint::Type Joint::getType() const {
    switch (b2Joint_GetType(b2LoadJointId(checkedId()))) {
    case b2_distanceJoint:
        return Type::Distance;
    case b2_revoluteJoint:
        return Type::Revolute;
    case b2_prismaticJoint:
        return Type::Prismatic;
    case b2_weldJoint:
        return Type::Weld;
    case b2_wheelJoint:
        return Type::Wheel;
    case b2_motorJoint:
        return world->mouseJoints.contains(id) ? Type::Mouse : Type::Motor;
    default:
        return Type::Filter;
    }
}

Body Joint::getBodyA() const {
    return {world, b2StoreBodyId(b2Joint_GetBodyA(b2LoadJointId(checkedId())))};
}

Body Joint::getBodyB() const {
    return {world, b2StoreBodyId(b2Joint_GetBodyB(b2LoadJointId(checkedId())))};
}

math::Vec2 Joint::getAnchorA() const {
    const b2JointId joint = b2LoadJointId(checkedId());
    return Box2DConverter::toPixels(b2Body_GetWorldPoint(b2Joint_GetBodyA(joint), b2Joint_GetLocalFrameA(joint).p), world->getPixelsPerMeter());
}

math::Vec2 Joint::getAnchorB() const {
    const b2JointId joint = b2LoadJointId(checkedId());
    return Box2DConverter::toPixels(b2Body_GetWorldPoint(b2Joint_GetBodyB(joint), b2Joint_GetLocalFrameB(joint).p), world->getPixelsPerMeter());
}

math::Vec2 Joint::getConstraintForce() const {
    return Box2DConverter::toPixels(b2Joint_GetConstraintForce(b2LoadJointId(checkedId())), getScale());
}

float Joint::getConstraintTorque() const {
    return b2Joint_GetConstraintTorque(b2LoadJointId(checkedId())) * getScale() * getScale();
}

float Joint::getLinearSeparation() const {
    return b2Joint_GetLinearSeparation(b2LoadJointId(checkedId())) * getScale();
}

float Joint::getAngularSeparation() const {
    return b2Joint_GetAngularSeparation(b2LoadJointId(checkedId()));
}

float Joint::getConstraintHertz() const {
    float hertz = 0.0F;
    float dampingRatio = 0.0F;
    b2Joint_GetConstraintTuning(b2LoadJointId(checkedId()), &hertz, &dampingRatio);
    return hertz;
}

void Joint::setConstraintHertz(float value) {
    b2Joint_SetConstraintTuning(b2LoadJointId(checkedId()), Box2DConverter::toPositive(value, "A joint needs a positive constraint stiffness."), getConstraintDampingRatio());
}

float Joint::getConstraintDampingRatio() const {
    float hertz = 0.0F;
    float dampingRatio = 0.0F;
    b2Joint_GetConstraintTuning(b2LoadJointId(checkedId()), &hertz, &dampingRatio);
    return dampingRatio;
}

void Joint::setConstraintDampingRatio(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A joint needs a finite constraint damping ratio of zero or more.");
    }
    b2Joint_SetConstraintTuning(b2LoadJointId(checkedId()), getConstraintHertz(), value);
}

// Box2D reports an awake joint whose load reaches its threshold, so a new limit wakes the joint, and a joint without a limit has the largest float as its threshold.
std::optional<float> Joint::getBreakForce() const {
    const float threshold = b2Joint_GetForceThreshold(b2LoadJointId(checkedId()));
    return threshold < FLT_MAX ? std::optional(threshold * getScale()) : std::nullopt;
}

void Joint::setBreakForce(std::optional<float> value) {
    const b2JointId joint = b2LoadJointId(checkedId());
    if (value && !(*value >= 0.0F)) {
        throw std::invalid_argument("A joint needs a break force and a break torque of zero or more.");
    }
    b2Joint_SetForceThreshold(joint, value ? *value / getScale() : FLT_MAX);
    b2Joint_WakeBodies(joint);
}

std::optional<float> Joint::getBreakTorque() const {
    const float threshold = b2Joint_GetTorqueThreshold(b2LoadJointId(checkedId()));
    return threshold < FLT_MAX ? std::optional(threshold * getScale() * getScale()) : std::nullopt;
}

void Joint::setBreakTorque(std::optional<float> value) {
    const b2JointId joint = b2LoadJointId(checkedId());
    if (value && !(*value >= 0.0F)) {
        throw std::invalid_argument("A joint needs a break force and a break torque of zero or more.");
    }
    b2Joint_SetTorqueThreshold(joint, value ? *value / (getScale() * getScale()) : FLT_MAX);
    b2Joint_WakeBodies(joint);
}

// The target of a mouse joint is the frame of the joint on the first body.
math::Vec2 Joint::getTarget() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Mouse}, "Only mouse joints have a target."));
    return Box2DConverter::toPixels(b2Body_GetWorldPoint(b2Joint_GetBodyA(joint), b2Joint_GetLocalFrameA(joint).p), getScale());
}

void Joint::setTarget(math::Vec2 value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Mouse}, "Only mouse joints have a target."));
    b2Joint_SetLocalFrameA(joint, {.p = b2Body_GetLocalPoint(b2Joint_GetBodyA(joint), Box2DConverter::toMeters(value, getScale())), .q = b2Rot_identity});
    b2Joint_WakeBodies(joint);
}

float Joint::getAngle() const {
    return b2RevoluteJoint_GetCurrentAngle(b2LoadJointId(checkedId({Type::Revolute}, "Only revolute joints have an angle.")));
}

float Joint::getTranslation() const {
    return b2PrismaticJoint_GetCurrentTranslation(b2LoadJointId(checkedId({Type::Prismatic}, "Only prismatic joints have a translation."))) * getScale();
}

float Joint::getCurrentLength() const {
    return b2DistanceJoint_GetCurrentLength(b2LoadJointId(checkedId({Type::Distance}, "Only distance joints have a current length."))) * getScale();
}

bool Joint::isLimitEnabled() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have limits."));
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_IsLimitEnabled(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_IsLimitEnabled(joint);
    case Type::Wheel:
        return b2WheelJoint_IsLimitEnabled(joint);
    default:
        return b2DistanceJoint_IsLimitEnabled(joint);
    }
}

void Joint::setLimitEnabled(bool value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have limits."));
    switch (getType()) {
    case Type::Revolute:
        b2RevoluteJoint_EnableLimit(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_EnableLimit(joint, value);
        break;
    case Type::Wheel:
        b2WheelJoint_EnableLimit(joint, value);
        break;
    default:
        b2DistanceJoint_EnableLimit(joint, value);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getLower() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have limits."));
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_GetLowerLimit(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_GetLowerLimit(joint) * getScale();
    case Type::Wheel:
        return b2WheelJoint_GetLowerLimit(joint) * getScale();
    default:
        return b2DistanceJoint_GetLowerLimit(joint) * getScale();
    }
}

float Joint::getUpper() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have limits."));
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_GetUpperLimit(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_GetUpperLimit(joint) * getScale();
    case Type::Wheel:
        return b2WheelJoint_GetUpperLimit(joint) * getScale();
    default:
        return b2DistanceJoint_GetUpperLimit(joint) * getScale();
    }
}

void Joint::setLimits(float lower, float upper) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have limits."));
    if (!(lower <= upper)) {
        throw std::invalid_argument("A joint needs a lower limit that is not above the upper one.");
    }
    switch (getType()) {
    case Type::Revolute:
        if (lower < -World::kRevoluteLimit || upper > World::kRevoluteLimit) {
            throw std::invalid_argument("A revolute joint needs a lower limit that is not above the upper one, both within 0.99 pi radians of zero.");
        }
        b2RevoluteJoint_SetLimits(joint, lower, upper);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_SetLimits(joint, lower / getScale(), upper / getScale());
        break;
    case Type::Wheel:
        b2WheelJoint_SetLimits(joint, lower / getScale(), upper / getScale());
        break;
    default:
        b2DistanceJoint_SetLimits(joint, std::max(lower / getScale(), Box2DConverter::kLinearSlop), std::max(upper / getScale(), Box2DConverter::kLinearSlop));
        break;
    }
    b2Joint_WakeBodies(joint);
}

bool Joint::isMotorEnabled() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have a motor."));
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_IsMotorEnabled(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_IsMotorEnabled(joint);
    case Type::Wheel:
        return b2WheelJoint_IsMotorEnabled(joint);
    default:
        return b2DistanceJoint_IsMotorEnabled(joint);
    }
}

void Joint::setMotorEnabled(bool value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have a motor."));
    switch (getType()) {
    case Type::Revolute:
        b2RevoluteJoint_EnableMotor(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_EnableMotor(joint, value);
        break;
    case Type::Wheel:
        b2WheelJoint_EnableMotor(joint, value);
        break;
    default:
        b2DistanceJoint_EnableMotor(joint, value);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getMotorSpeed() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have a motor speed."));
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_GetTargetMotorSpeed(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_GetTargetMotorSpeed(joint) * getScale();
    case Type::Wheel:
        return b2WheelJoint_GetTargetMotorSpeed(joint);
    default:
        return b2DistanceJoint_GetTargetMotorSpeed(joint) * getScale();
    }
}

void Joint::setMotorSpeed(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Prismatic, Type::Wheel, Type::Distance}, "Only revolute, prismatic, wheel and distance joints have a motor speed."));
    switch (getType()) {
    case Type::Revolute:
        b2RevoluteJoint_SetTargetMotorSpeed(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_SetTargetMotorSpeed(joint, value / getScale());
        break;
    case Type::Wheel:
        b2WheelJoint_SetTargetMotorSpeed(joint, value);
        break;
    default:
        b2DistanceJoint_SetTargetMotorSpeed(joint, value / getScale());
        break;
    }

    // Box2D does not wake sleeping bodies when only a motor target changes.
    b2Joint_WakeBodies(joint);
}

float Joint::getMaxMotorForce() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Prismatic, Type::Distance, Type::Mouse, Type::Motor}, "Only prismatic, distance, mouse and motor joints have a maximum motor force."));
    switch (getType()) {
    case Type::Prismatic:
        return b2PrismaticJoint_GetMaxMotorForce(joint) * getScale();
    case Type::Distance:
        return b2DistanceJoint_GetMaxMotorForce(joint) * getScale();
    default:
        return b2MotorJoint_GetMaxSpringForce(joint) * getScale();
    }
}

void Joint::setMaxMotorForce(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Prismatic, Type::Distance, Type::Mouse, Type::Motor}, "Only prismatic, distance, mouse and motor joints have a maximum motor force."));
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A joint needs a finite maximum motor force of zero or more.");
    }
    const float force = value / getScale();
    switch (getType()) {
    case Type::Prismatic:
        b2PrismaticJoint_SetMaxMotorForce(joint, force);
        break;
    case Type::Distance:
        b2DistanceJoint_SetMaxMotorForce(joint, force);
        break;
    case Type::Mouse:
        b2MotorJoint_SetMaxSpringForce(joint, force);
        b2MotorJoint_SetMaxVelocityTorque(joint, World::getMouseTurnFriction(b2StoreBodyId(b2Joint_GetBodyB(joint)), force));
        break;
    default:
        b2MotorJoint_SetMaxSpringForce(joint, force);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getMaxMotorTorque() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Wheel, Type::Motor}, "Only revolute, wheel and motor joints have a maximum motor torque."));
    const float scale = getScale() * getScale();
    switch (getType()) {
    case Type::Revolute:
        return b2RevoluteJoint_GetMaxMotorTorque(joint) * scale;
    case Type::Wheel:
        return b2WheelJoint_GetMaxMotorTorque(joint) * scale;
    default:
        return b2MotorJoint_GetMaxSpringTorque(joint) * scale;
    }
}

void Joint::setMaxMotorTorque(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Wheel, Type::Motor}, "Only revolute, wheel and motor joints have a maximum motor torque."));
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A joint needs a finite maximum motor torque of zero or more.");
    }
    const float torque = value / (getScale() * getScale());
    switch (getType()) {
    case Type::Revolute:
        b2RevoluteJoint_SetMaxMotorTorque(joint, torque);
        break;
    case Type::Wheel:
        b2WheelJoint_SetMaxMotorTorque(joint, torque);
        break;
    default:
        b2MotorJoint_SetMaxSpringTorque(joint, torque);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getMotorForce() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Prismatic, Type::Distance}, "Only prismatic and distance joints report a motor force."));
    return (getType() == Type::Prismatic ? b2PrismaticJoint_GetCurrentMotorForce(joint) : b2DistanceJoint_GetCurrentMotorForce(joint)) * getScale();
}

float Joint::getMotorTorque() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute, Type::Wheel}, "Only revolute and wheel joints report a motor torque."));
    return (getType() == Type::Revolute ? b2RevoluteJoint_GetCurrentMotorTorque(joint) : b2WheelJoint_GetCurrentMotorTorque(joint)) * getScale() * getScale();
}

bool Joint::isSpringEnabled() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel, Type::Mouse, Type::Motor, Type::Weld}, "Only distance, revolute, prismatic, wheel, mouse, motor and weld joints have a spring."));
    switch (getType()) {
    case Type::Distance:
        return b2DistanceJoint_IsSpringEnabled(joint);
    case Type::Revolute:
        return b2RevoluteJoint_IsSpringEnabled(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_IsSpringEnabled(joint);
    case Type::Wheel:
        return b2WheelJoint_IsSpringEnabled(joint);
    default:
        return true;
    }
}

void Joint::setSpringEnabled(bool value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel}, "Only distance, revolute, prismatic and wheel joints switch their spring."));
    switch (getType()) {
    case Type::Distance:
        b2DistanceJoint_EnableSpring(joint, value);
        break;
    case Type::Revolute:
        b2RevoluteJoint_EnableSpring(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_EnableSpring(joint, value);
        break;
    default:
        b2WheelJoint_EnableSpring(joint, value);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getHertz() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel, Type::Mouse, Type::Motor, Type::Weld}, "Only distance, revolute, prismatic, wheel, mouse, motor and weld joints have a spring."));
    switch (getType()) {
    case Type::Distance:
        return b2DistanceJoint_GetSpringHertz(joint);
    case Type::Revolute:
        return b2RevoluteJoint_GetSpringHertz(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_GetSpringHertz(joint);
    case Type::Wheel:
        return b2WheelJoint_GetSpringHertz(joint);
    case Type::Mouse:
    case Type::Motor:
        return b2MotorJoint_GetLinearHertz(joint);
    default:
        return b2WeldJoint_GetLinearHertz(joint);
    }
}

void Joint::setHertz(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel, Type::Mouse, Type::Motor, Type::Weld}, "Only distance, revolute, prismatic, wheel, mouse, motor and weld joints have a spring."));
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A joint spring needs a finite stiffness and damping ratio of zero or more.");
    }
    switch (getType()) {
    case Type::Distance:
        b2DistanceJoint_SetSpringHertz(joint, value);
        break;
    case Type::Revolute:
        b2RevoluteJoint_SetSpringHertz(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_SetSpringHertz(joint, value);
        break;
    case Type::Wheel:
        b2WheelJoint_SetSpringHertz(joint, value);
        break;
    case Type::Mouse:
        b2MotorJoint_SetLinearHertz(joint, value);
        break;
    case Type::Motor:
        b2MotorJoint_SetLinearHertz(joint, value);
        b2MotorJoint_SetAngularHertz(joint, value);
        break;
    default:
        b2WeldJoint_SetLinearHertz(joint, value);
        b2WeldJoint_SetAngularHertz(joint, value);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getDampingRatio() const {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel, Type::Mouse, Type::Motor, Type::Weld}, "Only distance, revolute, prismatic, wheel, mouse, motor and weld joints have a spring."));
    switch (getType()) {
    case Type::Distance:
        return b2DistanceJoint_GetSpringDampingRatio(joint);
    case Type::Revolute:
        return b2RevoluteJoint_GetSpringDampingRatio(joint);
    case Type::Prismatic:
        return b2PrismaticJoint_GetSpringDampingRatio(joint);
    case Type::Wheel:
        return b2WheelJoint_GetSpringDampingRatio(joint);
    case Type::Mouse:
    case Type::Motor:
        return b2MotorJoint_GetLinearDampingRatio(joint);
    default:
        return b2WeldJoint_GetLinearDampingRatio(joint);
    }
}

void Joint::setDampingRatio(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance, Type::Revolute, Type::Prismatic, Type::Wheel, Type::Mouse, Type::Motor, Type::Weld}, "Only distance, revolute, prismatic, wheel, mouse, motor and weld joints have a spring."));
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("A joint spring needs a finite stiffness and damping ratio of zero or more.");
    }
    switch (getType()) {
    case Type::Distance:
        b2DistanceJoint_SetSpringDampingRatio(joint, value);
        break;
    case Type::Revolute:
        b2RevoluteJoint_SetSpringDampingRatio(joint, value);
        break;
    case Type::Prismatic:
        b2PrismaticJoint_SetSpringDampingRatio(joint, value);
        break;
    case Type::Wheel:
        b2WheelJoint_SetSpringDampingRatio(joint, value);
        break;
    case Type::Mouse:
        b2MotorJoint_SetLinearDampingRatio(joint, value);
        break;
    case Type::Motor:
        b2MotorJoint_SetLinearDampingRatio(joint, value);
        b2MotorJoint_SetAngularDampingRatio(joint, value);
        break;
    default:
        b2WeldJoint_SetLinearDampingRatio(joint, value);
        b2WeldJoint_SetAngularDampingRatio(joint, value);
        break;
    }
    b2Joint_WakeBodies(joint);
}

float Joint::getLength() const {
    return b2DistanceJoint_GetRestLength(b2LoadJointId(checkedId({Type::Distance}, "Only distance joints have a length."))) * getScale();
}

void Joint::setLength(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Distance}, "Only distance joints have a length."));
    if (!(value / getScale() >= Box2DConverter::kLinearSlop)) {
        throw std::invalid_argument("A distance joint needs a length of at least 0.005 meters.");
    }
    b2DistanceJoint_SetRestLength(joint, value / getScale());
    b2Joint_WakeBodies(joint);
}

float Joint::getTargetAngle() const {
    return b2RevoluteJoint_GetTargetAngle(b2LoadJointId(checkedId({Type::Revolute}, "Only revolute joints have a target angle.")));
}

void Joint::setTargetAngle(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Revolute}, "Only revolute joints have a target angle."));
    b2RevoluteJoint_SetTargetAngle(joint, value);
    b2Joint_WakeBodies(joint);
}

float Joint::getTargetTranslation() const {
    return b2PrismaticJoint_GetTargetTranslation(b2LoadJointId(checkedId({Type::Prismatic}, "Only prismatic joints have a target translation."))) * getScale();
}

void Joint::setTargetTranslation(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Prismatic}, "Only prismatic joints have a target translation."));
    b2PrismaticJoint_SetTargetTranslation(joint, value / getScale());
    b2Joint_WakeBodies(joint);
}

// The offsets of a motor joint are the frame of the joint on the first body, which the spring pulls the origin of the second body toward.
math::Vec2 Joint::getLinearOffset() const {
    return Box2DConverter::toPixels(b2Joint_GetLocalFrameA(b2LoadJointId(checkedId({Type::Motor}, "Only motor joints have offsets."))).p, getScale());
}

void Joint::setLinearOffset(math::Vec2 value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Motor}, "Only motor joints have offsets."));
    b2Joint_SetLocalFrameA(joint, {.p = Box2DConverter::toMeters(value, getScale()), .q = b2Joint_GetLocalFrameA(joint).q});
    b2Joint_WakeBodies(joint);
}

float Joint::getAngularOffset() const {
    const b2Rot rotation = b2Joint_GetLocalFrameA(b2LoadJointId(checkedId({Type::Motor}, "Only motor joints have offsets."))).q;
    return std::atan2(rotation.s, rotation.c);
}

void Joint::setAngularOffset(float value) {
    const b2JointId joint = b2LoadJointId(checkedId({Type::Motor}, "Only motor joints have offsets."));
    b2Joint_SetLocalFrameA(joint, {.p = b2Joint_GetLocalFrameA(joint).p, .q = {.c = std::cos(value), .s = std::sin(value)}});
    b2Joint_WakeBodies(joint);
}

} // namespace haylen::physics2d
