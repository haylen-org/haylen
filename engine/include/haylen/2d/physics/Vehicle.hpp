#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A car with a box chassis on two wheels. Wheel joints hold the wheels on springy suspension along the vertical axis of the chassis, and their motors drive the car. The parts share a negative collision group, so the wheels never hit the chassis.
class Vehicle final {
  public:
    enum class Drive : std::uint8_t {
        Rear,
        Front,
        All,
    };

    // Wheel positions are offsets from the chassis center. The suspension travels up and down by suspensionTravel from where the wheels start.
    struct Options {
        math::Vec2 position{};
        math::Vec2 chassisSize{120.0F, 30.0F};
        float wheelRadius = 16.0F;
        math::Vec2 rearWheel{-40.0F, 20.0F};
        math::Vec2 frontWheel{40.0F, 20.0F};
        float density = 1.0F;
        float wheelDensity = 1.0F;
        float wheelFriction = 0.9F;
        float suspensionHertz = 5.0F;
        float suspensionDamping = 0.7F;
        float suspensionTravel = 10.0F;
        float maxMotorTorque = 50000.0F;
        Drive drive = Drive::Rear;
        int group = -2;
    };

    // Throws std::invalid_argument for a chassis or wheel without size, a negative suspension travel, a group that is not negative or an invalid material, and then leaves no bodies behind.
    [[nodiscard]] static Vehicle create(World& world, const Options& options);

    [[nodiscard]] static std::optional<Drive> driveFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view driveName(Drive value) noexcept;

    // Spins the driven wheels at this speed in radians per second, clockwise on screen for positive speeds, which drives the car toward positive x. A speed of zero brakes with the full motor torque.
    void setMotorSpeed(float radiansPerSecond);
    [[nodiscard]] float getMotorSpeed() const noexcept {
        return motorSpeed;
    }

    [[nodiscard]] Body getChassis() const noexcept {
        return chassis;
    }
    [[nodiscard]] Body getRearWheel() const noexcept {
        return wheels[0];
    }
    [[nodiscard]] Body getFrontWheel() const noexcept {
        return wheels[1];
    }
    [[nodiscard]] const std::array<Joint, 2>& getJoints() const noexcept {
        return suspension;
    }
    [[nodiscard]] Drive getDrive() const noexcept {
        return drive;
    }
    [[nodiscard]] bool isValid() const noexcept;

    void destroy();

  private:
    void build(World& world, const Options& options);

    Body chassis;
    std::array<Body, 2> wheels{};
    std::array<Joint, 2> suspension{};
    Drive drive = Drive::Rear;
    float motorSpeed = 0.0F;
};

} // namespace haylen::physics2d
