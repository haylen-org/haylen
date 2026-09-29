#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// Breaks polygons and bodies into pieces along Voronoi cells, such as a crate hit by a rocket or glass hit by a stone.
class Fracture final {
  public:
    // The cells grow around random points inside the shape, which gather around the impact point when there is one. Pieces smaller than minimumArea are dropped.
    struct Options {
        int pieces = 8;
        std::optional<math::Vec2> impact;
        std::uint64_t seed = 0;
        float minimumArea = 4.0F;
    };

    // Splits a shape, given as outlines like math::Polygon takes them, into pieces that together cover it. Each piece is a shape of its own. Throws std::invalid_argument when pieces is below 1.
    [[nodiscard]] static std::vector<std::vector<std::vector<math::Vec2>>> split(std::span<const std::vector<math::Vec2>> shape, const Options& options);

    // Replaces a body with one body per piece of its polygon shapes, which keep its type, material, filter and motion, and returns the new bodies. A body without polygon shapes stays as it is and gives no pieces. The impact point is in world units. Throws std::logic_error when the body was destroyed, and std::invalid_argument when a piece is too thin for Box2D, which leaves the body whole.
    static std::vector<Body> shatter(Body body, const Options& options);
};

} // namespace haylen::physics2d
