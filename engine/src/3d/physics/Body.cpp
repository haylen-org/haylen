#include "haylen/3d/physics/Body.hpp"

#include <cmath>
#include <format>
#include <stdexcept>

#include "3d/physics/WorldSlots.hpp"
#include "haylen/3d/physics/Simulation.hpp"
#include "haylen/3d/physics/World.hpp"

namespace haylen::physics3d {

const std::array<std::pair<std::string_view, Body::Type>, 3> Body::kTypeNames{{{"static", Type::Static}, {"kinematic", Type::Kinematic}, {"dynamic", Type::Dynamic}}};
const std::array<std::pair<std::string_view, Body::MotionQuality>, 2> Body::kMotionQualityNames{{{"discrete", MotionQuality::Discrete}, {"linearCast", MotionQuality::LinearCast}}};
const std::array<std::pair<std::string_view, Body::CombineRule>, 4> Body::kCombineRuleNames{{{"average", CombineRule::Average}, {"minimum", CombineRule::Minimum}, {"multiply", CombineRule::Multiply}, {"maximum", CombineRule::Maximum}}};

std::optional<Body::Type> Body::typeFromName(std::string_view name) noexcept {
    for (const auto& [text, value] : kTypeNames) {
        if (text == name) {
            return value;
        }
    }

    return std::nullopt;
}

std::string_view Body::typeName(Type value) noexcept {
    for (const auto& [text, each] : kTypeNames) {
        if (each == value) {
            return text;
        }
    }

    return kTypeNames.front().first;
}

std::optional<Body::MotionQuality> Body::motionQualityFromName(std::string_view name) noexcept {
    for (const auto& [text, value] : kMotionQualityNames) {
        if (text == name) {
            return value;
        }
    }

    return std::nullopt;
}

std::string_view Body::motionQualityName(MotionQuality value) noexcept {
    for (const auto& [text, each] : kMotionQualityNames) {
        if (each == value) {
            return text;
        }
    }

    return kMotionQualityNames.front().first;
}

std::optional<Body::CombineRule> Body::combineRuleFromName(std::string_view name) noexcept {
    for (const auto& [text, value] : kCombineRuleNames) {
        if (text == name) {
            return value;
        }
    }

    return std::nullopt;
}

std::string_view Body::combineRuleName(CombineRule value) noexcept {
    for (const auto& [text, each] : kCombineRuleNames) {
        if (each == value) {
            return text;
        }
    }

    return kCombineRuleNames.front().first;
}

void Body::requireNonNegative(std::string_view what, float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument(std::format("The {} of a 3D body must be a finite number of zero or more.", what));
    }
}

void Body::requireFinite(std::string_view what, float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::format("The {} of a 3D body must be a finite number.", what));
    }
}

void Body::requireFinite(std::string_view what, math::Vec3 value) {
    if (!value.isFinite()) {
        throw std::invalid_argument(std::format("The {} of a 3D body must be a vector of three finite numbers.", what));
    }
}

void Body::requirePositive(std::string_view what, math::Vec3 value) {
    if (!value.isFinite() || value.x <= 0.0F || value.y <= 0.0F || value.z <= 0.0F) {
        throw std::invalid_argument(std::format("The {} of a 3D body must be a vector of three finite numbers above zero.", what));
    }
}

void Body::requireRotation(std::string_view what, const math::Quat& value) {
    const float length = value.getLength();
    if (!std::isfinite(length) || std::abs(length - 1.0F) > kRotationTolerance) {
        throw std::invalid_argument(std::format("The {} of a 3D body must be a rotation, a quaternion of length one.", what));
    }
}

void Body::requireSpeed(math::Vec3 velocity, float limit, std::string_view what, std::string_view option) {
    if (velocity.getLength() > limit) {
        throw std::invalid_argument(std::format("The {} of a 3D body is faster than its {} of {:.6g}. Raise the {} or give a slower velocity.", what, option, limit, option));
    }
}

void Body::requireTransform(const Transform& value) {
    requireFinite("position", value.position);
    requireRotation("rotation", value.rotation);
}

void Body::requireShape(const std::shared_ptr<const Shape>& shape, Type type, std::string_view backend) {
    if (shape == nullptr) {
        throw std::invalid_argument("A 3D body needs a shape.");
    }

    if (shape->getBackendName() != backend) {
        throw std::invalid_argument(std::format("The shape of a 3D body was made by the 3D physics backend \"{}\", and the world takes only shapes of its own backend \"{}\".", shape->getBackendName(), backend));
    }

    if (type == Type::Dynamic && !shape->isSolid()) {
        throw std::invalid_argument(std::format("A dynamic 3D body needs a shape with a volume, and a {} has none. Make the body static or kinematic, or give it a convex hull or a compound of convex shapes.", Shape::kindName(shape->getKind())));
    }
}

void Body::requireFreeAxis(const MotionLocks& locks, Type type) {
    const bool everyAxis = locks.x && locks.y && locks.z && locks.rotationX && locks.rotationY && locks.rotationZ;
    if (type == Type::Dynamic && everyAxis) {
        throw std::invalid_argument("A dynamic 3D body locked on every axis never moves. Make it static, or leave one axis free.");
    }
}

void Body::Options::validate(std::string_view backend) const {
    requireShape(shape, type, backend);
    requireTransform({.position = position, .rotation = rotation});
    requireFinite("linear velocity", linearVelocity);
    requireFinite("angular velocity", angularVelocity);
    if (type == Type::Static && (!linearVelocity.isZero() || !angularVelocity.isZero())) {
        throw std::invalid_argument("A static 3D body never moves, so it takes no velocity. Make it kinematic to move it.");
    }

    requireFreeAxis(locks, type);
    if (mass && (!std::isfinite(*mass) || *mass <= 0.0F)) {
        throw std::invalid_argument("The mass of a 3D body must be a finite number above zero.");
    }

    if (inertia) {
        requirePositive("inertia", *inertia);
    }

    if (centerOfMass) {
        requireFinite("center of mass", *centerOfMass);
    }

    requireNonNegative("max linear speed", maxLinearSpeed);
    requireNonNegative("max angular speed", maxAngularSpeed);
    requireSpeed(linearVelocity, maxLinearSpeed, "linear velocity", "max linear speed");
    requireSpeed(angularVelocity, maxAngularSpeed, "angular velocity", "max angular speed");
    requireNonNegative("linear damping", linearDamping);
    requireNonNegative("angular damping", angularDamping);
    requireFinite("gravity scale", gravityScale);
    requireNonNegative("friction", friction);
    requireNonNegative("restitution", restitution);
}

World& Body::checkedWorld() const {
    if (world == nullptr || !WorldSlots::isLive(key)) {
        throw std::logic_error("The 3D physics world of the body was destroyed.");
    }

    if (!world->getSimulation().isBodyValid(id)) {
        throw std::logic_error("The 3D physics body was destroyed.");
    }

    return *world;
}

bool Body::isValid() const noexcept {
    return world != nullptr && WorldSlots::isLive(key) && world->simulation->isBodyValid(id);
}

void Body::destroy() {
    if (!isValid()) {
        return;
    }

    world->getSimulation().destroyBody(id);
    world->forgetBody(id);
}

void Body::requireDynamic(std::string_view action) const {
    if (getType() != Type::Dynamic) {
        throw std::logic_error(std::format("Only a dynamic 3D body {}.", action));
    }
}

void Body::requireMutableCompound() const {
    if (getShape()->getKind() != Shape::Kind::MutableCompound) {
        throw std::logic_error("Only a 3D body whose shape is a mutable compound has children of its own.");
    }
}

Body::Type Body::getType() const {
    return checkedWorld().getSimulation().getBodyType(id);
}

void Body::setType(Type value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireShape(simulation.getBodyShape(id), value, world->getBackendName());
    requireFreeAxis(simulation.getMotionLocks(id), value);
    simulation.setBodyType(id, value);
}

Body::MotionQuality Body::getMotionQuality() const {
    return checkedWorld().getSimulation().getMotionQuality(id);
}

void Body::setMotionQuality(MotionQuality value) {
    checkedWorld().getSimulation().setMotionQuality(id, value);
}

Body::MotionLocks Body::getMotionLocks() const {
    return checkedWorld().getSimulation().getMotionLocks(id);
}

void Body::setMotionLocks(const MotionLocks& value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFreeAxis(value, simulation.getBodyType(id));
    simulation.setMotionLocks(id, value);
}

math::Vec3 Body::getPosition() const {
    return getTransform().position;
}

math::Quat Body::getRotation() const {
    return getTransform().rotation;
}

Body::Transform Body::getTransform() const {
    return checkedWorld().getSimulation().getBodyTransform(id);
}

void Body::setPosition(math::Vec3 value) {
    setTransform({.position = value, .rotation = getRotation()});
}

void Body::setRotation(const math::Quat& value) {
    setTransform({.position = getPosition(), .rotation = value});
}

void Body::setTransform(const Transform& value) {
    World& owner = checkedWorld();
    requireTransform(value);
    owner.getSimulation().setBodyTransform(id, {.position = value.position, .rotation = value.rotation.getNormalized()});
    owner.forgetMotion(id);
}

void Body::moveTo(const Transform& target, float seconds) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireTransform(target);
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        throw std::invalid_argument("A 3D body moves to a transform over a time above zero seconds.");
    }

    if (simulation.getBodyType(id) == Type::Static) {
        throw std::logic_error("A static 3D body never moves. Make it kinematic to move it to a transform.");
    }

    simulation.moveBody(id, {.position = target.position, .rotation = target.rotation.getNormalized()}, seconds);
}

math::Vec3 Body::getLinearVelocity() const {
    return checkedWorld().getSimulation().getBodyVector(id, Vector::LinearVelocity);
}

void Body::setLinearVelocity(math::Vec3 value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFinite("linear velocity", value);
    if (simulation.getBodyType(id) == Type::Static) {
        throw std::logic_error("A static 3D body never moves, so it takes no velocity. Make it kinematic to move it.");
    }

    requireSpeed(value, simulation.getBodyValue(id, Value::MaxLinearSpeed), "linear velocity", "max linear speed");
    simulation.setBodyVector(id, Vector::LinearVelocity, value);
}

math::Vec3 Body::getAngularVelocity() const {
    return checkedWorld().getSimulation().getBodyVector(id, Vector::AngularVelocity);
}

void Body::setAngularVelocity(math::Vec3 value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFinite("angular velocity", value);
    if (simulation.getBodyType(id) == Type::Static) {
        throw std::logic_error("A static 3D body never moves, so it takes no velocity. Make it kinematic to move it.");
    }

    requireSpeed(value, simulation.getBodyValue(id, Value::MaxAngularSpeed), "angular velocity", "max angular speed");
    simulation.setBodyVector(id, Vector::AngularVelocity, value);
}

math::Vec3 Body::getVelocityAt(math::Vec3 point) const {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFinite("point", point);
    return simulation.getVelocityAt(id, point);
}

void Body::applyForce(math::Vec3 force, std::optional<math::Vec3> point) {
    requireDynamic("takes forces");
    requireFinite("force", force);
    if (point) {
        requireFinite("point", *point);
    }

    world->getSimulation().pushBody(id, Push::Force, force, point);
}

void Body::applyImpulse(math::Vec3 impulse, std::optional<math::Vec3> point) {
    requireDynamic("takes impulses");
    requireFinite("impulse", impulse);
    if (point) {
        requireFinite("point", *point);
    }

    world->getSimulation().pushBody(id, Push::Impulse, impulse, point);
}

void Body::applyTorque(math::Vec3 torque) {
    requireDynamic("takes torques");
    requireFinite("torque", torque);
    world->getSimulation().pushBody(id, Push::Torque, torque, std::nullopt);
}

void Body::applyAngularImpulse(math::Vec3 impulse) {
    requireDynamic("takes angular impulses");
    requireFinite("angular impulse", impulse);
    world->getSimulation().pushBody(id, Push::AngularImpulse, impulse, std::nullopt);
}

float Body::getMass() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::Mass);
}

void Body::setMass(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    if (!std::isfinite(value) || value <= 0.0F) {
        throw std::invalid_argument("The mass of a 3D body must be a finite number above zero.");
    }

    simulation.setBodyValue(id, Value::Mass, value);
}

math::Vec3 Body::getInertia() const {
    return checkedWorld().getSimulation().getBodyVector(id, Vector::Inertia);
}

void Body::setInertia(math::Vec3 value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requirePositive("inertia", value);
    simulation.setBodyVector(id, Vector::Inertia, value);
}

math::Vec3 Body::getCenterOfMass() const {
    return checkedWorld().getSimulation().getBodyVector(id, Vector::CenterOfMass);
}

void Body::setCenterOfMass(math::Vec3 value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFinite("center of mass", value);
    simulation.setBodyVector(id, Vector::CenterOfMass, value);
}

void Body::resetMass() {
    checkedWorld().getSimulation().resetMass(id);
}

math::Vec3 Body::getWorldCenter() const {
    return checkedWorld().getSimulation().getBodyVector(id, Vector::WorldCenter);
}

float Body::getLinearDamping() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::LinearDamping);
}

void Body::setLinearDamping(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("linear damping", value);
    simulation.setBodyValue(id, Value::LinearDamping, value);
}

float Body::getAngularDamping() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::AngularDamping);
}

void Body::setAngularDamping(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("angular damping", value);
    simulation.setBodyValue(id, Value::AngularDamping, value);
}

float Body::getMaxLinearSpeed() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::MaxLinearSpeed);
}

void Body::setMaxLinearSpeed(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("max linear speed", value);
    simulation.setBodyValue(id, Value::MaxLinearSpeed, value);
}

float Body::getMaxAngularSpeed() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::MaxAngularSpeed);
}

void Body::setMaxAngularSpeed(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("max angular speed", value);
    simulation.setBodyValue(id, Value::MaxAngularSpeed, value);
}

float Body::getFriction() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::Friction);
}

void Body::setFriction(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("friction", value);
    simulation.setBodyValue(id, Value::Friction, value);
}

float Body::getRestitution() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::Restitution);
}

void Body::setRestitution(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireNonNegative("restitution", value);
    simulation.setBodyValue(id, Value::Restitution, value);
}

float Body::getGravityScale() const {
    return checkedWorld().getSimulation().getBodyValue(id, Value::GravityScale);
}

void Body::setGravityScale(float value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireFinite("gravity scale", value);
    simulation.setBodyValue(id, Value::GravityScale, value);
}

Body::CombineRule Body::getFrictionCombine() const {
    return checkedWorld().getSimulation().getCombineRule(id, Material::Friction);
}

void Body::setFrictionCombine(CombineRule value) {
    checkedWorld().getSimulation().setCombineRule(id, Material::Friction, value);
}

Body::CombineRule Body::getRestitutionCombine() const {
    return checkedWorld().getSimulation().getCombineRule(id, Material::Restitution);
}

void Body::setRestitutionCombine(CombineRule value) {
    checkedWorld().getSimulation().setCombineRule(id, Material::Restitution, value);
}

bool Body::isSleepEnabled() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::SleepEnabled);
}

void Body::setSleepEnabled(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::SleepEnabled, value);
}

bool Body::isAwake() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::Awake);
}

void Body::setAwake(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::Awake, value);
}

bool Body::isSensor() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::Sensor);
}

void Body::setSensor(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::Sensor, value);
}

bool Body::isCollidingWithStatic() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::CollideWithStatic);
}

void Body::setCollideWithStatic(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::CollideWithStatic, value);
}

bool Body::hasEnhancedInternalEdgeRemoval() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::EnhancedInternalEdgeRemoval);
}

void Body::setEnhancedInternalEdgeRemoval(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::EnhancedInternalEdgeRemoval, value);
}

bool Body::isEnabled() const {
    return checkedWorld().getSimulation().getBodyFlag(id, Flag::Enabled);
}

void Body::setEnabled(bool value) {
    checkedWorld().getSimulation().setBodyFlag(id, Flag::Enabled, value);
}

std::shared_ptr<const Shape> Body::getShape() const {
    return checkedWorld().getSimulation().getBodyShape(id);
}

void Body::setShape(std::shared_ptr<const Shape> value) {
    Simulation& simulation = checkedWorld().getSimulation();
    requireShape(value, simulation.getBodyType(id), world->getBackendName());
    simulation.setBodyShape(id, value);
}

math::Box Body::getBounds() const {
    return checkedWorld().getSimulation().getBodyBounds(id);
}

std::size_t Body::getChildCount() const {
    requireMutableCompound();
    return world->getSimulation().getChildCount(id);
}

std::size_t Body::addChild(const Shape::Child& child) {
    requireMutableCompound();
    if (child.shape == nullptr) {
        throw std::invalid_argument("A child of a 3D body needs a shape.");
    }

    requireShape(child.shape, getType(), world->getBackendName());
    if (child.shape->getKind() == Shape::Kind::Plane) {
        throw std::invalid_argument("A plane stands alone and is never the child of a mutable compound. Give the plane a body of its own.");
    }

    requireTransform({.position = child.position, .rotation = child.rotation});
    return world->getSimulation().addChild(id, {.shape = child.shape, .position = child.position, .rotation = child.rotation.getNormalized()});
}

void Body::removeChild(std::size_t index) {
    requireMutableCompound();
    Simulation& simulation = world->getSimulation();
    if (index >= simulation.getChildCount(id)) {
        throw std::out_of_range(std::format("The 3D body has no child at the index {}.", index));
    }

    if (simulation.getChildCount(id) == 1 && simulation.getBodyType(id) == Type::Dynamic) {
        throw std::logic_error("A dynamic 3D body keeps at least one child, since a body without a volume has no mass. Make the body static or kinematic first, or add another child before you remove this one.");
    }

    simulation.removeChild(id, index);
}

void Body::setChildTransform(std::size_t index, const Transform& value) {
    requireMutableCompound();
    requireTransform(value);
    Simulation& simulation = world->getSimulation();
    if (index >= simulation.getChildCount(id)) {
        throw std::out_of_range(std::format("The 3D body has no child at the index {}.", index));
    }

    simulation.setChildTransform(id, index, {.position = value.position, .rotation = value.rotation.getNormalized()});
}

} // namespace haylen::physics3d
