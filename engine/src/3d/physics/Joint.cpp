#include "haylen/3d/physics/Joint.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

#include "3d/physics/JointValidator.hpp"
#include "3d/physics/WorldSlots.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics3d {

const std::array<std::pair<std::string_view, Joint::Type>, 12> Joint::kTypeNames{{
    {"fixed", Type::Fixed},
    {"point", Type::Point},
    {"distance", Type::Distance},
    {"hinge", Type::Hinge},
    {"slider", Type::Slider},
    {"cone", Type::Cone},
    {"swingTwist", Type::SwingTwist},
    {"sixDof", Type::SixDof},
    {"path", Type::Path},
    {"gear", Type::Gear},
    {"rackAndPinion", Type::RackAndPinion},
    {"pulley", Type::Pulley},
}};
const std::array<std::pair<std::string_view, Joint::AxisMode>, 3> Joint::kAxisModeNames{{{"free", AxisMode::Free}, {"limited", AxisMode::Limited}, {"fixed", AxisMode::Fixed}}};
const std::array<std::pair<std::string_view, Joint::Axis>, 6> Joint::kAxisNames{{{"x", Axis::X}, {"y", Axis::Y}, {"z", Axis::Z}, {"rotationX", Axis::RotationX}, {"rotationY", Axis::RotationY}, {"rotationZ", Axis::RotationZ}}};
const std::array<std::pair<std::string_view, Joint::SpringMode>, 2> Joint::kSpringModeNames{{{"frequency", SpringMode::Frequency}, {"stiffness", SpringMode::Stiffness}}};
const std::array<std::pair<std::string_view, Joint::MotorState>, 3> Joint::kMotorStateNames{{{"off", MotorState::Off}, {"velocity", MotorState::Velocity}, {"position", MotorState::Position}}};
const std::array<std::pair<std::string_view, Joint::PathRotation>, 6> Joint::kPathRotationNames{{{"free", PathRotation::Free}, {"aroundTangent", PathRotation::AroundTangent}, {"aroundNormal", PathRotation::AroundNormal}, {"aroundBinormal", PathRotation::AroundBinormal}, {"withPath", PathRotation::WithPath}, {"fixed", PathRotation::Fixed}}};

template <typename Enum, std::size_t Count> std::optional<Enum> Joint::fromName(const std::array<std::pair<std::string_view, Enum>, Count>& names, std::string_view name) noexcept {
    for (const auto& [text, value] : names) {
        if (text == name) {
            return value;
        }
    }

    return std::nullopt;
}

template <typename Enum, std::size_t Count> std::string_view Joint::nameOf(const std::array<std::pair<std::string_view, Enum>, Count>& names, Enum value) noexcept {
    for (const auto& [text, each] : names) {
        if (each == value) {
            return text;
        }
    }

    return names.front().first;
}

std::optional<Joint::Type> Joint::typeFromName(std::string_view name) noexcept {
    return fromName(kTypeNames, name);
}

std::string_view Joint::typeName(Type value) noexcept {
    return nameOf(kTypeNames, value);
}

std::optional<Joint::AxisMode> Joint::axisModeFromName(std::string_view name) noexcept {
    return fromName(kAxisModeNames, name);
}

std::string_view Joint::axisModeName(AxisMode value) noexcept {
    return nameOf(kAxisModeNames, value);
}

std::optional<Joint::Axis> Joint::axisFromName(std::string_view name) noexcept {
    return fromName(kAxisNames, name);
}

std::string_view Joint::axisName(Axis value) noexcept {
    return nameOf(kAxisNames, value);
}

std::optional<Joint::SpringMode> Joint::springModeFromName(std::string_view name) noexcept {
    return fromName(kSpringModeNames, name);
}

std::string_view Joint::springModeName(SpringMode value) noexcept {
    return nameOf(kSpringModeNames, value);
}

std::optional<Joint::MotorState> Joint::motorStateFromName(std::string_view name) noexcept {
    return fromName(kMotorStateNames, name);
}

std::string_view Joint::motorStateName(MotorState value) noexcept {
    return nameOf(kMotorStateNames, value);
}

std::optional<Joint::PathRotation> Joint::pathRotationFromName(std::string_view name) noexcept {
    return fromName(kPathRotationNames, name);
}

std::string_view Joint::pathRotationName(PathRotation value) noexcept {
    return nameOf(kPathRotationNames, value);
}

World& Joint::checkedWorld() const {
    if (world == nullptr || !WorldSlots::isLive(key)) {
        throw std::logic_error("The 3D physics world of the joint was destroyed.");
    }

    if (!world->getSimulation().isJointValid(id)) {
        throw std::logic_error("The 3D physics joint was destroyed.");
    }

    return *world;
}

World& Joint::checkedWorld(std::initializer_list<Type> types, std::string_view member) const {
    World& owner = checkedWorld();
    const Type type = owner.getSimulation().getJointType(id);
    if (std::ranges::find(types, type) != types.end()) {
        return owner;
    }

    std::string names;
    for (const Type each : types) {
        names += names.empty() ? std::format("\"{}\"", typeName(each)) : std::format(", \"{}\"", typeName(each));
    }

    throw std::logic_error(std::format("Only the joints {} have {}, and this joint is \"{}\".", names, member, typeName(type)));
}

bool Joint::isValid() const noexcept {
    return world != nullptr && WorldSlots::isLive(key) && world->simulation->isJointValid(id);
}

void Joint::destroy() {
    if (!isValid()) {
        return;
    }

    world->destroyJoint(id);
}

Joint::Type Joint::getType() const {
    return checkedWorld().getSimulation().getJointType(id);
}

Body Joint::getFirst() const {
    World& owner = checkedWorld();
    return {&owner, key, owner.getSimulation().getJointBodies(id).first};
}

std::optional<Body> Joint::getSecond() const {
    World& owner = checkedWorld();
    const std::optional<Simulation::BodyId> second = owner.getSimulation().getJointBodies(id).second;
    if (!second) {
        return std::nullopt;
    }

    return Body{&owner, key, *second};
}

bool Joint::isEnabled() const {
    return checkedWorld().getSimulation().isJointEnabled(id);
}

void Joint::setEnabled(bool value) {
    checkedWorld().getSimulation().setJointEnabled(id, value);
}

int Joint::getPriority() const {
    return checkedWorld().getSimulation().getJointPriority(id);
}

void Joint::setPriority(int value) {
    checkedWorld().getSimulation().setJointPriority(id, value);
}

Joint::Load Joint::getLoad() const {
    return checkedWorld().getSimulation().getJointLoad(id);
}

std::optional<float> Joint::getBreakForce() const {
    return checkedWorld().getBreakLimits(id).force;
}

void Joint::setBreakForce(std::optional<float> value) {
    World& owner = checkedWorld();
    JointValidator::requireBreakLimit(value);
    World::BreakLimits limits = owner.getBreakLimits(id);
    limits.force = value;
    owner.setBreakLimits(id, limits);
}

std::optional<float> Joint::getBreakTorque() const {
    return checkedWorld().getBreakLimits(id).torque;
}

void Joint::setBreakTorque(std::optional<float> value) {
    World& owner = checkedWorld();
    JointValidator::requireBreakLimit(value);
    World::BreakLimits limits = owner.getBreakLimits(id);
    limits.torque = value;
    owner.setBreakLimits(id, limits);
}

float Joint::getLower() const {
    return checkedWorld({Type::Hinge, Type::Slider, Type::Distance, Type::SwingTwist, Type::Pulley}, "limits").getSimulation().getJointValue(id, Value::Lower);
}

float Joint::getUpper() const {
    return checkedWorld({Type::Hinge, Type::Slider, Type::Distance, Type::SwingTwist, Type::Pulley}, "limits").getSimulation().getJointValue(id, Value::Upper);
}

void Joint::setLimits(float lower, float upper) {
    World& owner = checkedWorld({Type::Hinge, Type::Slider, Type::Distance, Type::SwingTwist, Type::Pulley}, "limits");
    JointValidator::requireLimits(owner.getSimulation().getJointType(id), lower, upper);
    owner.getSimulation().setJointLimits(id, lower, upper);
}

Joint::Spring Joint::getLimitSpring() const {
    return checkedWorld({Type::Hinge, Type::Slider, Type::Distance}, "a limit spring").getSimulation().getLimitSpring(id);
}

void Joint::setLimitSpring(const Spring& value) {
    World& owner = checkedWorld({Type::Hinge, Type::Slider, Type::Distance}, "a limit spring");
    JointValidator::requireSpring(value);
    owner.getSimulation().setLimitSpring(id, value);
}

float Joint::getFriction() const {
    return checkedWorld({Type::Hinge, Type::Slider, Type::SwingTwist, Type::Path}, "friction").getSimulation().getJointValue(id, Value::Friction);
}

void Joint::setFriction(float value) {
    World& owner = checkedWorld({Type::Hinge, Type::Slider, Type::SwingTwist, Type::Path}, "friction");
    JointValidator::requireNonNegative("friction", value);
    owner.getSimulation().setJointValue(id, Value::Friction, value);
}

float Joint::getHalfConeAngle() const {
    return checkedWorld({Type::Cone, Type::SwingTwist}, "a cone").getSimulation().getJointValue(id, Value::HalfConeAngle);
}

void Joint::setHalfConeAngle(float value) {
    World& owner = checkedWorld({Type::Cone, Type::SwingTwist}, "a cone");
    JointValidator::requireConeAngle(value);
    owner.getSimulation().setJointValue(id, Value::HalfConeAngle, value);
}

float Joint::getPlaneHalfConeAngle() const {
    return checkedWorld({Type::SwingTwist}, "a plane cone").getSimulation().getJointValue(id, Value::PlaneHalfConeAngle);
}

void Joint::setPlaneHalfConeAngle(float value) {
    World& owner = checkedWorld({Type::SwingTwist}, "a plane cone");
    JointValidator::requireConeAngle(value);
    owner.getSimulation().setJointValue(id, Value::PlaneHalfConeAngle, value);
}

float Joint::getRatio() const {
    return checkedWorld({Type::Gear, Type::RackAndPinion, Type::Pulley}, "a ratio").getSimulation().getJointValue(id, Value::Ratio);
}

float Joint::getAngle() const {
    return checkedWorld({Type::Hinge}, "an angle").getSimulation().getJointValue(id, Value::Angle);
}

float Joint::getPosition() const {
    return checkedWorld({Type::Slider}, "a position").getSimulation().getJointValue(id, Value::Position);
}

float Joint::getLength() const {
    return checkedWorld({Type::Distance}, "a length").getSimulation().getJointValue(id, Value::Length);
}

float Joint::getFraction() const {
    return checkedWorld({Type::Path}, "a fraction").getSimulation().getJointValue(id, Value::Fraction);
}

Joint::Motor Joint::getMotor(MotorSlot slot) const {
    World& owner = checkedWorld();
    JointValidator::requireMotorSlot(owner.getSimulation().getJointType(id), slot);
    return owner.getSimulation().getJointMotor(id, slot);
}

void Joint::setMotor(const Motor& value, MotorSlot slot) {
    World& owner = checkedWorld();
    JointValidator::requireMotorSlot(owner.getSimulation().getJointType(id), slot);
    JointValidator::requireMotor(value);
    owner.getSimulation().setJointMotor(id, slot, value);
}

math::Quat Joint::getTargetRotation() const {
    return checkedWorld({Type::SwingTwist, Type::SixDof}, "a target rotation").getSimulation().getTargetRotation(id);
}

void Joint::setTargetRotation(const math::Quat& value) {
    World& owner = checkedWorld({Type::SwingTwist, Type::SixDof}, "a target rotation");
    JointValidator::requireRotation("target rotation", value);
    owner.getSimulation().setTargetRotation(id, value.getNormalized());
}

math::Vec3 Joint::getTargetAngularVelocity() const {
    return checkedWorld({Type::SwingTwist}, "a target angular velocity").getSimulation().getTargetAngularVelocity(id);
}

void Joint::setTargetAngularVelocity(math::Vec3 value) {
    World& owner = checkedWorld({Type::SwingTwist}, "a target angular velocity");
    JointValidator::requireFinite("target angular velocity", value);
    owner.getSimulation().setTargetAngularVelocity(id, value);
}

Joint::AxisSettings Joint::getAxis(Axis axis) const {
    return checkedWorld({Type::SixDof}, "axes of their own").getSimulation().getJointAxis(id, axis);
}

void Joint::setAxis(Axis axis, const AxisSettings& value) {
    World& owner = checkedWorld({Type::SixDof}, "axes of their own");
    JointValidator::requireAxis(axis, value);
    owner.getSimulation().setJointAxis(id, axis, value);
}

} // namespace haylen::physics3d
