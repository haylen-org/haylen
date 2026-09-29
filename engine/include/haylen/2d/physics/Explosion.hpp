#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Pushes the dynamic bodies around a point away from it, like a blast.
class Explosion final {
  public:
    enum class Falloff : std::uint8_t {
        None,
        Linear,
        Quadratic,
    };

    // Each body takes the impulse, scaled by the falloff over its distance, at the point of its shapes closest to the center, so bodies hit off center spin. With occlusion, bodies behind other shapes as seen from the center are spared.
    struct Options {
        math::Vec2 center{};
        float radius = 100.0F;
        float impulse = 500.0F;
        Falloff falloff = Falloff::Linear;
        bool occlusion = false;
        CollisionFilter filter{};
    };

    struct Hit {
        Body body;
        math::Vec2 point{};
        math::Vec2 impulse{};
    };

    // Returns the bodies it pushed with the impulses they took. Throws std::invalid_argument when the radius is not positive.
    static std::vector<Hit> apply(World& world, const Options& options);

    [[nodiscard]] static std::optional<Falloff> falloffFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view falloffName(Falloff value) noexcept;

  private:
    [[nodiscard]] static float scaleAt(const Options& options, float distance) noexcept;
};

} // namespace haylen::physics2d
