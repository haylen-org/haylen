#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A human figure seen from the side, made of eleven capsules joined by revolute joints with the angle limits of real joints. Its parts share a negative collision group, so they never collide with each other, and collide with the rest of the world through its category and mask.
class Ragdoll final {
  public:
    enum class Part : std::uint8_t {
        Head,
        Chest,
        Hips,
        UpperArmLeft,
        LowerArmLeft,
        UpperArmRight,
        LowerArmRight,
        UpperLegLeft,
        LowerLegLeft,
        UpperLegRight,
        LowerLegRight,
    };

    static constexpr std::size_t kPartCount = 11;

    // The position is the center of the hips and `height` runs from the top of the head to the feet. Stiffness from 0 to 1 is how much every joint resists bending: a joint of stiffness 1 holds the limb below it straight out against standard gravity, whatever the size of the figure and the scale of the world, and 0 leaves the figure limp.
    struct Options {
        math::Vec2 position{};
        float height = 128.0F;
        float density = 1.0F;
        float friction = 0.6F;
        float stiffness = 0.2F;
        CollisionFilter filter{.group = -1};
        math::Vec2 velocity{};
    };

    // Throws `std::invalid_argument` when the height is not positive, the group is not negative, the stiffness is outside 0 to 1 or the material is invalid, and then leaves no bodies behind.
    [[nodiscard]] static Ragdoll create(World& world, const Options& options);

    [[nodiscard]] static std::optional<Part> partFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view partName(Part value) noexcept;

    [[nodiscard]] Body getBody(Part part) const noexcept {
        return bodies[static_cast<std::size_t>(part)];
    }
    [[nodiscard]] const std::array<Body, kPartCount>& getBodies() const noexcept {
        return bodies;
    }
    [[nodiscard]] const std::vector<Joint>& getJoints() const noexcept {
        return joints;
    }
    [[nodiscard]] float getMass() const;
    [[nodiscard]] bool isValid() const noexcept;

    void destroy();

  private:
    // Capsule ends and radius in heights, relative to the center of the hips.
    struct Bone {
        float top = 0.0F;
        float bottom = 0.0F;
        float radius = 0.0F;
    };

    // A joint at a height, relative to the center of the hips, with its angle limits in radians.
    struct Link {
        Part parent = Part::Chest;
        Part child = Part::Head;
        float at = 0.0F;
        float lower = 0.0F;
        float upper = 0.0F;
    };

    static const std::array<std::string_view, kPartCount> kNames;
    static const std::array<Bone, kPartCount> kBones;
    static const std::array<Link, kPartCount - 1> kLinks;

    // Standard gravity in meters per second squared, which the stiffness measures the joints against.
    static constexpr float kStandardGravity = 9.80665F;

    void build(World& world, const Options& options);
    // Adds the mass of the part and of every part below it, and returns how far below the joint at `at` they reach, in heights.
    float measureLimb(Part part, float at, float& mass) const;

    std::array<Body, kPartCount> bodies{};
    std::vector<Joint> joints;
};

} // namespace haylen::physics2d
