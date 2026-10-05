#include "haylen/2d/physics/Vehicle.hpp"

#include <box2d/box2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Vehicle::Drive>, 3> Vehicle::kDriveNames{{{"rear", Drive::Rear}, {"front", Drive::Front}, {"all", Drive::All}}};

std::optional<Vehicle::Drive> Vehicle::driveFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kDriveNames, name, &std::pair<std::string_view, Drive>::first);
    return found != kDriveNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Vehicle::driveName(Drive value) noexcept {
    return std::ranges::find(kDriveNames, value, &std::pair<std::string_view, Drive>::second)->first;
}

Vehicle::Vehicle(World& owner, const Options& settings) : world(owner), options(settings) {
    const bool sized = options.chassisSize.x > 0.0F && options.chassisSize.y > 0.0F && options.wheelRadius > 0.0F;
    const bool driven = options.acceleration > 0.0F && options.topSpeed > 0.0F && options.brakeAcceleration > 0.0F;
    if (!sized || !(options.suspensionTravel >= 0.0F) || options.filter.group >= 0 || !driven || !(options.antiRoll >= 0.0F && options.antiRoll <= 1.0F) || !(options.airControl >= 0.0F)) {
        throw std::invalid_argument("A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more, a negative collision group, a positive acceleration, top speed and brake, air control of zero or more and an anti-roll from 0 to 1.");
    }

    // A vehicle that fails halfway, such as on a bad material, leaves no bodies behind.
    try {
        build();
    } catch (...) {
        destroy();
        throw;
    }
    hook = world.addStepHook(World::StepPhase::Before, [this](float deltaSeconds) { update(deltaSeconds); });
}

Vehicle::~Vehicle() {
    world.removeStepHook(hook);
    destroy();
}

void Vehicle::build() {
    chassis = world.createBody({.position = options.position, .bullet = options.bullet});
    chassis.addBox(options.chassisSize, {.density = options.density, .filter = options.filter});

    axles = {options.rearWheel, options.frontWheel};
    for (std::size_t index = 0; index < axles.size(); ++index) {
        const math::Vec2 center = options.position + axles[index];
        Body& wheel = wheels[index];
        wheel = world.createBody({.position = center, .bullet = options.bullet, .fastRotation = true});
        wheel.addCircle(options.wheelRadius, {.density = options.wheelDensity, .friction = options.wheelFriction, .filter = options.filter});
        const Joint::Options joint{
            .anchorA = center,
            .enableLimit = true,
            .lower = -options.suspensionTravel,
            .upper = options.suspensionTravel,
            .enableMotor = true,
            .enableSpring = true,
            .hertz = options.suspensionHertz,
            .dampingRatio = options.suspensionDamping,
            .axis = {0.0F, 1.0F},
        };
        suspension[index] = world.createJoint(Joint::Type::Wheel, chassis, wheel, joint);
    }

    // A low center of mass keeps the car on its wheels, so it sits at the height of the axles unless the options place it.
    const float axleHeight = (options.rearWheel.y + options.frontWheel.y) * 0.5F;
    chassis.setCenterOfMass(options.centerOfMass.value_or(math::Vec2{0.0F, axleHeight}));

    const float mass = chassis.getMass() + wheels[0].getMass() + wheels[1].getMass();
    const float drivenWheels = options.drive == Drive::All ? 2.0F : 1.0F;
    driveTorque = mass * options.acceleration * options.wheelRadius / drivenWheels;
    brakeTorque = mass * options.brakeAcceleration * options.wheelRadius * 0.5F;
    applyControls();
}

bool Vehicle::isDriven(std::size_t wheel) const noexcept {
    return options.drive == Drive::All || (options.drive == Drive::Rear) == (wheel == 0);
}

void Vehicle::setThrottle(float value) {
    throttle = std::clamp(value, -1.0F, 1.0F);
    applyControls();
}

void Vehicle::setBrake(float value) {
    brake = std::clamp(value, 0.0F, 1.0F);
    applyControls();
}

// The motors aim at the top speed with the torque the throttle asks for, so the car accelerates at most at its acceleration. Without throttle or brake the motors only hold back the wheels a little, so the car rolls on and slows down slowly.
void Vehicle::applyControls() {
    constexpr float kRollingDrag = 0.03F;
    const float wheelSpeed = options.topSpeed / options.wheelRadius;
    for (std::size_t index = 0; index < suspension.size(); ++index) {
        Joint& joint = suspension[index];
        if (brake > 0.0F) {
            joint.setMotorSpeed(0.0F);
            joint.setMaxMotorTorque(brake * brakeTorque);
        } else if (throttle != 0.0F && isDriven(index)) {
            joint.setMotorSpeed(std::copysign(wheelSpeed, throttle));
            joint.setMaxMotorTorque(std::abs(throttle) * driveTorque);
        } else {
            joint.setMotorSpeed(0.0F);
            joint.setMaxMotorTorque(kRollingDrag * driveTorque);
        }
    }
}

// Box2D lists only the contacts that touch, so one is enough.
bool Vehicle::isGrounded() const {
    // clang-format off
    return std::ranges::any_of(wheels, [](const Body& wheel) {
        b2ContactData contact{};
        return wheel.isValid() && b2Body_GetContactData(b2LoadBodyId(wheel.getId()), &contact, 1) > 0;
    });
    // clang-format on
}

float Vehicle::getSpeed() const {
    return math::Vec2::dot(chassis.getVelocity(), math::Vec2::fromAngle(chassis.getRotation()));
}

// The compression of a wheel is how far it rose along the suspension axis from where it started.
float Vehicle::getCompression(std::size_t wheel) const {
    const b2BodyId body = b2LoadBodyId(chassis.getId());
    const float scale = world.getPixelsPerMeter();
    const b2Vec2 anchor = b2Body_GetWorldPoint(body, {axles[wheel].x / scale, axles[wheel].y / scale});
    const b2Vec2 axis = b2Body_GetWorldVector(body, {0.0F, 1.0F});
    const b2Vec2 offset = b2Sub(b2Body_GetPosition(b2LoadBodyId(wheels[wheel].getId())), anchor);
    return -b2Dot(offset, axis);
}

void Vehicle::update(float) {
    if (!isValid()) {
        return;
    }
    const b2BodyId body = b2LoadBodyId(chassis.getId());
    if (!isGrounded()) {
        if (throttle != 0.0F) {
            // The throttle lifts the nose in the air, which turns the chassis counter-clockwise on screen.
            b2Body_ApplyTorque(body, -throttle * options.airControl * b2Body_GetRotationalInertia(body), true);
        }
        return;
    }
    if (options.antiRoll <= 0.0F) {
        return;
    }

    // The coupling pushes the more compressed axle up and the other one down, like a bar between the springs, which resists pitching and leaves bumps that lift both wheels alone.
    const float omega = math::Math::kTau * options.suspensionHertz;
    const float stiffness = options.antiRoll * b2Body_GetMass(body) * 0.5F * omega * omega;
    const float force = stiffness * (getCompression(1) - getCompression(0));
    const b2Vec2 axis = b2Body_GetWorldVector(body, {0.0F, 1.0F});
    const float scale = world.getPixelsPerMeter();
    for (std::size_t index = 0; index < wheels.size(); ++index) {
        const float sign = index == 1 ? 1.0F : -1.0F;
        const b2Vec2 push = b2MulSV(sign * force, axis);
        const b2Vec2 anchor = b2Body_GetWorldPoint(body, {axles[index].x / scale, axles[index].y / scale});
        b2Body_ApplyForce(body, b2Neg(push), anchor, true);
        b2Body_ApplyForceToCenter(b2LoadBodyId(wheels[index].getId()), push, true);
    }
}

bool Vehicle::isValid() const noexcept {
    return chassis.isValid() && wheels[0].isValid() && wheels[1].isValid();
}

void Vehicle::destroy() {
    chassis.destroy();
    for (Body& wheel : wheels) {
        wheel.destroy();
    }
}

} // namespace haylen::physics2d
