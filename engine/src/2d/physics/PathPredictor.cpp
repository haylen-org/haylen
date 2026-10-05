#include "haylen/2d/physics/PathPredictor.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

PathPredictor::Path PathPredictor::predict(const World& world, math::Vec2 position, math::Vec2 velocity, const Options& options) {
    if (!(options.step > 0.0F) || options.steps < 1 || !(options.radius >= 0.0F) || !(options.linearDamping >= 0.0F)) {
        throw std::invalid_argument("A path prediction needs a positive step, at least one step, and a radius and damping of zero or more.");
    }

    const Raycaster raycaster(world);
    const int subSteps = world.getSubSteps();
    const float subStep = options.step / static_cast<float>(subSteps);
    const math::Vec2 gravity = world.getGravity() * (options.gravityScale * subStep);
    const float damping = 1.0F / (1.0F + subStep * options.linearDamping);
    const float maxSpeed = world.getMaxSpeed();

    Path path;
    path.points.reserve(static_cast<std::size_t>(options.steps) + 1);
    path.points.push_back(position);
    for (int step = 0; step < options.steps; ++step) {
        // The solver adds gravity to the damped velocity and then moves the body, once per sub-step.
        const math::Vec2 start = position;
        for (int sub = 0; sub < subSteps; ++sub) {
            velocity = (gravity + velocity * damping).clampedLength(maxSpeed);
            position += velocity * subStep;
        }
        const Raycaster::Filter filter{.collision = options.filter};
        path.hit = options.radius > 0.0F ? raycaster.castCircle(start, options.radius, position - start, filter) : raycaster.castRay(start, position, filter);
        if (path.hit) {
            path.points.push_back(options.radius > 0.0F ? start + (position - start) * path.hit->fraction : path.hit->point);
            return path;
        }
        path.points.push_back(position);
    }
    return path;
}

} // namespace haylen::physics2d
