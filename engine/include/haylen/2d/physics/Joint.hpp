#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Handle to a joint between two bodies, valid until either the joint or one of its bodies is destroyed.
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
        math::Vec2 axis{1.0F, 0.0F};
        float length = 0.0F;
    };

    Joint() = default;
    Joint(World* owner, std::uint64_t handle) noexcept : world(owner), id(handle) {}

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    void destroy();

    // Moves the target of a mouse joint.
    void setTarget(math::Vec2 value);

    // Changes the motor speed of a revolute, prismatic or wheel joint.
    void setMotorSpeed(float value);

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 8> kTypeNames;

    [[nodiscard]] std::uint64_t checkedId() const;

    World* world = nullptr;
    std::uint64_t id = 0;
};

} // namespace haylen::physics2d
