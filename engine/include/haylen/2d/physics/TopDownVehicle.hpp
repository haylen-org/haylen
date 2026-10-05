#pragma once

#include <cstdint>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Vehicle.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A car seen from above on a world without gravity: a box body with a front and a rear axle whose tires grip sideways up to a limit, so it steers, drifts when a turn asks for more grip than the tires have, skids on the handbrake and slows down by rolling drag. It faces its positive x axis at rotation 0. The vehicle owns its body and acts on it every step of its world, so it must go before its world.
class TopDownVehicle final {
  public:
    // The axles are distances along the car from its center, front positive. Accelerations are in world units per second squared and speeds in world units per second. Grip is the sideways acceleration the tires hold before they slide, and the handbrake keeps `handbrakeGrip` of the rear grip. Steering turns the front tires up to `steeringLock` radians at `steeringSpeed` radians per second, and the lock shrinks toward `highSpeedLock` of itself at the top speed. Rolling drag slows the car by that share of its speed every second.
    struct Options {
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 size{80.0F, 40.0F};
        float density = 1.0F;
        float friction = 0.3F;
        float restitution = 0.2F;
        float frontAxle = 26.0F;
        float rearAxle = -26.0F;
        float acceleration = 700.0F;
        float reverseAcceleration = 350.0F;
        float topSpeed = 900.0F;
        float reverseSpeed = 300.0F;
        float brakeAcceleration = 1500.0F;
        float grip = 1400.0F;
        float handbrakeGrip = 0.25F;
        float steeringLock = 0.6F;
        float steeringSpeed = 4.0F;
        float highSpeedLock = 0.4F;
        float rollingDrag = 0.4F;
        float angularDamping = 2.0F;
        Vehicle::Drive drive = Vehicle::Drive::Rear;
        CollisionFilter filter{};
        bool bullet = false;
    };

    // Throws `std::invalid_argument` for a car without size, an acceleration, speed, brake or grip that is not positive, a handbrake grip or high speed lock outside 0 to 1, a front axle that is not ahead of the rear one or an invalid material, and then leaves no body behind.
    TopDownVehicle(World& owner, const Options& settings);
    ~TopDownVehicle();

    TopDownVehicle(const TopDownVehicle&) = delete;
    TopDownVehicle& operator=(const TopDownVehicle&) = delete;

    // The throttle runs from -1, full reverse, to 1, full ahead, and the steering from -1, full left as seen from the driver, to 1, full right. The brake runs from 0 to 1. Values outside are clamped.
    void setThrottle(float value);
    [[nodiscard]] float getThrottle() const noexcept {
        return throttle;
    }
    void setSteering(float value);
    [[nodiscard]] float getSteering() const noexcept {
        return steering;
    }
    void setBrake(float value);
    [[nodiscard]] float getBrake() const noexcept {
        return brake;
    }
    void setHandbrake(bool value) noexcept {
        handbrake = value;
    }
    [[nodiscard]] bool isHandbrake() const noexcept {
        return handbrake;
    }

    // The angle the front tires turn now, the speed along the car, positive forward, and the sideways speed of the rear axle, which grows while the car drifts.
    [[nodiscard]] float getSteeringAngle() const noexcept {
        return steeringAngle;
    }
    [[nodiscard]] float getSpeed() const;
    [[nodiscard]] float getSlip() const;
    // Tells whether a tire slid in the last step because the turn asked for more grip than it has.
    [[nodiscard]] bool isDrifting() const noexcept {
        return drifting;
    }

    [[nodiscard]] Body getBody() const noexcept {
        return body;
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    [[nodiscard]] bool isValid() const noexcept {
        return body.isValid();
    }
    void destroy();

  private:
    // Cancels the sideways speed of one axle up to its grip and drives or brakes it, and returns whether it slid.
    bool updateAxle(float distance, float angle, float grip, bool driven, float deltaSeconds);
    void update(float deltaSeconds);

    World& world;
    Options options;
    Body body;
    float throttle = 0.0F;
    float steering = 0.0F;
    float brake = 0.0F;
    bool handbrake = false;
    float steeringAngle = 0.0F;
    bool drifting = false;
    std::uint64_t hook = 0;
};

} // namespace haylen::physics2d
