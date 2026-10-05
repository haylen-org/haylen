#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class Body;
class World;

// Handle to a joint between two bodies, valid until either the joint or one of its bodies is destroyed. Every setting a joint type has at creation can change while it runs, and the members a joint type does not have throw `std::logic_error` with the joint types that have them.
class Joint final {
  public:
    // A filter joint only keeps its two bodies from colliding with each other, without holding them together.
    enum class Type : std::uint8_t {
        Distance,
        Revolute,
        Prismatic,
        Weld,
        Wheel,
        Mouse,
        Motor,
        Filter,
    };

    // A joint breaks once its force passes `breakForce` or its torque passes `breakTorque`, in world units, and never without them. The spring of a revolute joint pulls toward `targetAngle` and that of a prismatic joint toward `targetTranslation`.
    struct Options {
        math::Vec2 anchorA{};
        math::Vec2 anchorB{};
        bool collideConnected = false;
        bool enableLimit = false;
        float lower = 0.0F;
        float upper = 0.0F;
        bool enableMotor = false;
        float motorSpeed = 0.0F;
        float maxMotorForce = 0.0F;
        float maxMotorTorque = 0.0F;
        bool enableSpring = false;
        float hertz = 0.0F;
        float dampingRatio = 0.0F;
        float targetAngle = 0.0F;
        float targetTranslation = 0.0F;
        math::Vec2 axis{1.0F, 0.0F};
        float length = 0.0F;
        std::optional<float> breakForce;
        std::optional<float> breakTorque;
    };

    Joint() = default;
    // A handle remembers the generation of its world, so it turns invalid once that world is destroyed, even after a new world reuses its slot.
    Joint(World* owner, std::uint64_t handle) noexcept;

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    void destroy();

    [[nodiscard]] Type getType() const;
    [[nodiscard]] Body getBodyA() const;
    [[nodiscard]] Body getBodyB() const;
    // Returns where the joint holds each body in world space now, which is where a rope or a spring between them is drawn.
    [[nodiscard]] math::Vec2 getAnchorA() const;
    [[nodiscard]] math::Vec2 getAnchorB() const;

    // The force and torque the joint applied in the last step to hold its bodies, and how far its anchors and angles drifted apart, in world units.
    [[nodiscard]] math::Vec2 getConstraintForce() const;
    [[nodiscard]] float getConstraintTorque() const;
    [[nodiscard]] float getLinearSeparation() const;
    [[nodiscard]] float getAngularSeparation() const;

    // The stiffness and damping that hold the joint together, 60 hertz and a damping ratio of 2 by default. Lower values make the joint soft.
    [[nodiscard]] float getConstraintHertz() const;
    void setConstraintHertz(float value);
    [[nodiscard]] float getConstraintDampingRatio() const;
    void setConstraintDampingRatio(float value);

    // Throws `std::invalid_argument` for a negative limit.
    [[nodiscard]] std::optional<float> getBreakForce() const;
    void setBreakForce(std::optional<float> value);
    [[nodiscard]] std::optional<float> getBreakTorque() const;
    void setBreakTorque(std::optional<float> value);

    // Mouse joints only.
    [[nodiscard]] math::Vec2 getTarget() const;
    void setTarget(math::Vec2 value);

    // Revolute joints read their angle from the pose at creation, prismatic joints their translation along the axis and distance joints their length now.
    [[nodiscard]] float getAngle() const;
    [[nodiscard]] float getTranslation() const;
    [[nodiscard]] float getCurrentLength() const;

    // Revolute, prismatic, wheel and distance joints. Distance joints limit their length. Throws `std::invalid_argument` for a lower limit above the upper one, and for revolute limits beyond 0.99 pi radians.
    [[nodiscard]] bool isLimitEnabled() const;
    void setLimitEnabled(bool value);
    [[nodiscard]] float getLower() const;
    [[nodiscard]] float getUpper() const;
    void setLimits(float lower, float upper);

    // Revolute, prismatic, wheel and distance joints. Revolute and wheel motors turn in radians per second, and prismatic and distance motors move in world units per second.
    [[nodiscard]] bool isMotorEnabled() const;
    void setMotorEnabled(bool value);
    [[nodiscard]] float getMotorSpeed() const;
    void setMotorSpeed(float value);

    // Prismatic and distance motors, mouse joints and motor joints.
    [[nodiscard]] float getMaxMotorForce() const;
    void setMaxMotorForce(float value);
    // Revolute and wheel motors and motor joints.
    [[nodiscard]] float getMaxMotorTorque() const;
    void setMaxMotorTorque(float value);
    // The force or torque the motor used in the last step.
    [[nodiscard]] float getMotorForce() const;
    [[nodiscard]] float getMotorTorque() const;

    // Distance, revolute, prismatic and wheel joints have springs that can be switched on, and mouse and weld joints are always springy, where a weld joint with 0 hertz is rigid.
    [[nodiscard]] bool isSpringEnabled() const;
    void setSpringEnabled(bool value);
    [[nodiscard]] float getHertz() const;
    void setHertz(float value);
    [[nodiscard]] float getDampingRatio() const;
    void setDampingRatio(float value);

    // Distance joints only. Throws `std::invalid_argument` for a length shorter than 0.005 meters.
    [[nodiscard]] float getLength() const;
    void setLength(float value);

    // Revolute joints only.
    [[nodiscard]] float getTargetAngle() const;
    void setTargetAngle(float value);

    // Prismatic joints only.
    [[nodiscard]] float getTargetTranslation() const;
    void setTargetTranslation(float value);

    // Motor joints only: the offset of the second body from the first in the space of the first, and the angle between them.
    [[nodiscard]] math::Vec2 getLinearOffset() const;
    void setLinearOffset(math::Vec2 value);
    [[nodiscard]] float getAngularOffset() const;
    void setAngularOffset(float value);

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] World* getWorld() const noexcept {
        return world;
    }
    [[nodiscard]] bool operator==(const Joint& other) const noexcept {
        return worldHandle == other.worldHandle && id == other.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 8> kTypeNames;

    [[nodiscard]] std::uint64_t checkedId() const;
    // Returns the id when the joint is one of the types, and throws `std::logic_error` with the message otherwise.
    [[nodiscard]] std::uint64_t checkedId(std::initializer_list<Type> types, const char* message) const;
    [[nodiscard]] float getScale() const noexcept;

    World* world = nullptr;
    std::uint32_t worldHandle = 0;
    std::uint64_t id = 0;
};

} // namespace haylen::physics2d
