#pragma once

#include <optional>
#include <vector>

#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/RaycastHit.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Predicts the flight of a thrown body the way the solver of the world integrates it, sub-step by sub-step with gravity, gravity scale and linear damping, and sweeps it through the world to the first shape it would hit, which draws the aim of a slingshot that matches the real throw.
class PathPredictor final {
  public:
    // The step is the time of one world step, and the path keeps one point per step. The radius sweeps a circle, and 0 sweeps a ray.
    struct Options {
        float step = 1.0F / 60.0F;
        int steps = 120;
        float radius = 0.0F;
        float gravityScale = 1.0F;
        float linearDamping = 0.0F;
        CollisionFilter filter{};
    };

    // The points start at the launch position and end at the hit when there is one.
    struct Path {
        std::vector<math::Vec2> points;
        std::optional<RaycastHit> hit;
    };

    // Throws `std::invalid_argument` for a step that is not positive, fewer than one step, or a negative radius or damping.
    [[nodiscard]] static Path predict(const World& world, math::Vec2 position, math::Vec2 velocity, const Options& options);
};

} // namespace haylen::physics2d
