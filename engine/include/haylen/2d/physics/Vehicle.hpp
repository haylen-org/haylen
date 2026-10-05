#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A car seen from the side: a box chassis on two wheels held by wheel joints on springy suspension along the vertical axis of the chassis. The motors size their torque from the mass of the car and the acceleration it asks for, so the car accelerates, brakes and coasts without lifting its nose or flipping. The center of mass sits at the height of the axles, the wheels may spin fast, an anti-roll coupling between the axles resists pitching, and the throttle turns the car in the air. The parts share a negative collision group, so the wheels never hit the chassis. The vehicle owns its bodies and acts on them every step of its world, so it must go before its world.
class Vehicle final {
  public:
    enum class Drive : std::uint8_t {
        Rear,
        Front,
        All,
    };

    // Wheel positions are offsets from the chassis center, and the suspension travels up and down by `suspensionTravel` from where the wheels start. Accelerations are in world units per second squared and the top speed in world units per second at the rim of the wheels. The center of mass is an offset from the chassis center, the height of the axles when unset. Air control is the angular acceleration in radians per second squared that a full throttle gives the chassis while no wheel touches the ground. Anti-roll from 0 to 1 couples the springs of the two axles, which keeps the chassis level when it speeds up and slows down.
    struct Options {
        math::Vec2 position{};
        math::Vec2 chassisSize{120.0F, 30.0F};
        float wheelRadius = 16.0F;
        math::Vec2 rearWheel{-40.0F, 20.0F};
        math::Vec2 frontWheel{40.0F, 20.0F};
        float density = 2.0F;
        float wheelDensity = 1.0F;
        float wheelFriction = 0.9F;
        float suspensionHertz = 5.0F;
        float suspensionDamping = 0.7F;
        float suspensionTravel = 10.0F;
        float acceleration = 600.0F;
        float topSpeed = 900.0F;
        float brakeAcceleration = 1600.0F;
        std::optional<math::Vec2> centerOfMass;
        float airControl = 6.0F;
        float antiRoll = 0.5F;
        Drive drive = Drive::Rear;
        CollisionFilter filter{.group = -2};
        bool bullet = false;
    };

    // Throws `std::invalid_argument` for a chassis or wheel without size, a negative suspension travel, a group that is not negative, an acceleration, top speed or brake that is not positive, an anti-roll outside 0 to 1 or an invalid material, and then leaves no bodies behind.
    Vehicle(World& owner, const Options& settings);
    ~Vehicle();

    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    [[nodiscard]] static std::optional<Drive> driveFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view driveName(Drive value) noexcept;

    // The throttle runs from -1, full reverse, to 1, full ahead, and 0 lets the car roll. The brake runs from 0 to 1 and holds the wheels with that share of the brake. Values outside are clamped.
    void setThrottle(float value);
    [[nodiscard]] float getThrottle() const noexcept {
        return throttle;
    }
    void setBrake(float value);
    [[nodiscard]] float getBrake() const noexcept {
        return brake;
    }

    // Tells whether a wheel touched the ground in the last step.
    [[nodiscard]] bool isGrounded() const;
    // Returns the speed of the chassis along its own axis in world units per second, positive when it drives toward its front.
    [[nodiscard]] float getSpeed() const;

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
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    [[nodiscard]] bool isValid() const noexcept;

    void destroy();

  private:
    static const std::array<std::pair<std::string_view, Drive>, 3> kDriveNames;

    void build();
    [[nodiscard]] bool isDriven(std::size_t wheel) const noexcept;
    // Sets the motors from the throttle and the brake.
    void applyControls();
    // Turns the chassis by the throttle in the air and couples the springs of the axles on the ground.
    void update(float deltaSeconds);
    [[nodiscard]] float getCompression(std::size_t wheel) const;

    World& world;
    Options options;
    Body chassis;
    std::array<Body, 2> wheels{};
    std::array<Joint, 2> suspension{};
    std::array<math::Vec2, 2> axles{};
    float throttle = 0.0F;
    float brake = 0.0F;
    float driveTorque = 0.0F;
    float brakeTorque = 0.0F;
    std::uint64_t hook = 0;
};

} // namespace haylen::physics2d
