#include "haylen/2d/physics/TopDownVehicle.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

TopDownVehicle::TopDownVehicle(World& owner, const Options& settings) : world(owner), options(settings) {
    const bool sized = options.size.x > 0.0F && options.size.y > 0.0F && options.frontAxle > options.rearAxle;
    const bool driven = options.acceleration > 0.0F && options.reverseAcceleration > 0.0F && options.topSpeed > 0.0F && options.reverseSpeed > 0.0F && options.brakeAcceleration > 0.0F && options.grip > 0.0F;
    const bool shares = options.handbrakeGrip >= 0.0F && options.handbrakeGrip <= 1.0F && options.highSpeedLock >= 0.0F && options.highSpeedLock <= 1.0F;
    if (!sized || !driven || !shares || !(options.steeringLock >= 0.0F) || !(options.steeringSpeed > 0.0F) || !(options.rollingDrag >= 0.0F)) {
        throw std::invalid_argument("A top-down vehicle needs a size, a front axle ahead of the rear one, positive accelerations, speeds, brake, grip and steering speed, a steering lock and rolling drag of zero or more, and a handbrake grip and high speed lock from 0 to 1.");
    }

    body = world.createBody({.position = options.position, .rotation = options.rotation, .angularDamping = options.angularDamping, .bullet = options.bullet});
    try {
        body.addBox(options.size, {.density = options.density, .friction = options.friction, .restitution = options.restitution, .filter = options.filter});
    } catch (...) {
        body.destroy();
        throw;
    }
    hook = world.addStepHook(World::StepPhase::Before, [this](float deltaSeconds) { update(deltaSeconds); });
}

TopDownVehicle::~TopDownVehicle() {
    world.removeStepHook(hook);
    destroy();
}

void TopDownVehicle::setThrottle(float value) {
    throttle = std::clamp(value, -1.0F, 1.0F);
    if (throttle != 0.0F && body.isValid()) {
        body.setAwake(true);
    }
}

void TopDownVehicle::setSteering(float value) {
    steering = std::clamp(value, -1.0F, 1.0F);
}

void TopDownVehicle::setBrake(float value) {
    brake = std::clamp(value, 0.0F, 1.0F);
}

float TopDownVehicle::getSpeed() const {
    return math::Vec2::dot(body.getVelocity(), math::Vec2::fromAngle(body.getRotation()));
}

float TopDownVehicle::getSlip() const {
    const float rotation = body.getRotation();
    const math::Vec2 rear = body.getPosition() + math::Vec2::fromAngle(rotation, options.rearAxle);
    return math::Vec2::dot(body.getVelocityAt(rear), math::Vec2::fromAngle(rotation).getPerpendicular());
}

bool TopDownVehicle::updateAxle(float distance, float angle, float grip, bool driven, float deltaSeconds) {
    const float rotation = body.getRotation();
    const math::Vec2 forward = math::Vec2::fromAngle(rotation + angle);
    const math::Vec2 side = forward.getPerpendicular();
    const math::Vec2 point = body.getPosition() + math::Vec2::fromAngle(rotation, distance);
    const math::Vec2 velocity = body.getVelocityAt(point);
    const float mass = body.getMass() * 0.5F;

    // The tire cancels its sideways speed up to its grip and slides past it.
    const float lateral = -math::Vec2::dot(velocity, side) * mass;
    const float hold = grip * mass * deltaSeconds;
    body.applyImpulse(side * std::clamp(lateral, -hold, hold), point);
    const bool slid = std::abs(lateral) > hold;

    // Each driven axle pushes its share of the whole car.
    const float speed = math::Vec2::dot(velocity, forward);
    const float pushed = body.getMass() * (options.drive == Vehicle::Drive::All ? 0.5F : 1.0F);
    if (driven && throttle > 0.0F && speed < options.topSpeed) {
        body.applyForce(forward * (throttle * options.acceleration * pushed), point);
    } else if (driven && throttle < 0.0F && speed > -options.reverseSpeed) {
        body.applyForce(forward * (throttle * options.reverseAcceleration * pushed), point);
    }
    if (brake > 0.0F && speed != 0.0F) {
        const float stop = std::min(brake * options.brakeAcceleration * deltaSeconds, std::abs(speed)) * mass;
        body.applyImpulse(forward * std::copysign(stop, -speed), point);
    }
    return slid;
}

void TopDownVehicle::update(float deltaSeconds) {
    if (!body.isValid()) {
        return;
    }

    // The steering lock shrinks with speed, so the car stays drivable when it is fast.
    const float speed = getSpeed();
    const float fast = std::clamp(std::abs(speed) / options.topSpeed, 0.0F, 1.0F);
    const float target = steering * options.steeringLock * (1.0F + (options.highSpeedLock - 1.0F) * fast);
    const float turn = options.steeringSpeed * deltaSeconds;
    steeringAngle += std::clamp(target - steeringAngle, -turn, turn);

    const bool frontDriven = options.drive != Vehicle::Drive::Rear;
    const bool rearDriven = options.drive != Vehicle::Drive::Front;
    const bool frontSlid = updateAxle(options.frontAxle, steeringAngle, options.grip, frontDriven, deltaSeconds);
    const bool rearSlid = updateAxle(options.rearAxle, 0.0F, options.grip * (handbrake ? options.handbrakeGrip : 1.0F), rearDriven, deltaSeconds);
    drifting = frontSlid || rearSlid;

    const math::Vec2 forward = math::Vec2::fromAngle(body.getRotation());
    const float drag = std::min(1.0F, options.rollingDrag * deltaSeconds);
    body.applyImpulse(forward * (-math::Vec2::dot(body.getVelocity(), forward) * body.getMass() * drag));
}

void TopDownVehicle::destroy() {
    body.destroy();
}

} // namespace haylen::physics2d
