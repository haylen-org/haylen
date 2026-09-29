#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Joint.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A human figure seen from the side, made of eleven capsules joined by revolute joints with the angle limits of real joints. Its parts share a negative collision group, so they never collide with each other.
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

    // The position is the center of the hips and height runs from the top of the head to the feet. Joint friction is the torque that resists bending, which makes the figure less limp.
    struct Options {
        math::Vec2 position{};
        float height = 128.0F;
        float density = 1.0F;
        float friction = 0.6F;
        float jointFriction = 0.0F;
        int group = -1;
        math::Vec2 velocity{};
    };

    // Throws std::invalid_argument when the height is not positive or the group is not negative.
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

    std::array<Body, kPartCount> bodies{};
    std::vector<Joint> joints;
};

} // namespace haylen::physics2d
