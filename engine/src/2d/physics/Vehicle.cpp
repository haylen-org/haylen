#include "haylen/2d/physics/Vehicle.hpp"

#include <stdexcept>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

std::optional<Vehicle::Drive> Vehicle::driveFromName(std::string_view name) noexcept {
    if (name == "rear") {
        return Drive::Rear;
    }
    if (name == "front") {
        return Drive::Front;
    }
    if (name == "all") {
        return Drive::All;
    }
    return std::nullopt;
}

std::string_view Vehicle::driveName(Drive value) noexcept {
    switch (value) {
    case Drive::Front:
        return "front";
    case Drive::All:
        return "all";
    case Drive::Rear:
        break;
    }
    return "rear";
}

Vehicle Vehicle::create(World& world, const Options& options) {
    if (options.chassisSize.x <= 0.0F || options.chassisSize.y <= 0.0F || options.wheelRadius <= 0.0F || !(options.suspensionTravel >= 0.0F) || options.group >= 0) {
        throw std::invalid_argument("A vehicle needs a chassis and wheels with a size, a suspension travel of zero or more and a negative collision group.");
    }

    // A vehicle that fails halfway, such as on a bad material, leaves no bodies behind.
    Vehicle vehicle;
    try {
        vehicle.build(world, options);
    } catch (...) {
        vehicle.destroy();
        throw;
    }
    return vehicle;
}

void Vehicle::build(World& world, const Options& options) {
    drive = options.drive;
    const CollisionFilter filter{.group = options.group};
    chassis = world.createBody({.position = options.position});
    chassis.addBox(options.chassisSize, {.density = options.density, .filter = filter});

    const std::array<math::Vec2, 2> offsets{options.rearWheel, options.frontWheel};
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const math::Vec2 center = options.position + offsets[index];
        Body& wheel = wheels[index];
        wheel = world.createBody({.position = center});
        wheel.addCircle(options.wheelRadius, {.density = options.wheelDensity, .friction = options.wheelFriction, .filter = filter});

        const bool driven = options.drive == Drive::All || (options.drive == Drive::Rear) == (index == 0);
        const Joint::Options joint{
            .anchorA = center,
            .enableLimit = true,
            .lower = -options.suspensionTravel,
            .upper = options.suspensionTravel,
            .enableMotor = driven,
            .maxMotorTorque = driven ? options.maxMotorTorque : 0.0F,
            .enableSpring = true,
            .hertz = options.suspensionHertz,
            .dampingRatio = options.suspensionDamping,
            .axis = {0.0F, 1.0F},
        };
        suspension[index] = world.createJoint(Joint::Type::Wheel, chassis, wheel, joint);
    }
}

void Vehicle::setMotorSpeed(float radiansPerSecond) {
    motorSpeed = radiansPerSecond;
    for (std::size_t index = 0; index < suspension.size(); ++index) {
        if (drive == Drive::All || (drive == Drive::Rear) == (index == 0)) {
            suspension[index].setMotorSpeed(radiansPerSecond);
        }
    }

    // A parked vehicle falls asleep, and Box2D does not wake it when only a motor target changes.
    chassis.setAwake(true);
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
