#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A liquid made of small circle bodies that collide with the world, held together and spread apart by the double density relaxation of particle fluids. World::readTransforms reads the positions of its bodies in bulk for metaball rendering. The fluid owns its particle bodies and must go before its world, and a particle whose body is destroyed elsewhere, such as in a contact callback, leaves the fluid the next time it is updated, counted or read.
class Fluid final {
  public:
    // Particles interact within the smoothing radius. Pressure pulls them toward the rest density, near pressure keeps them from clumping and viscosity evens out their speeds. Stiffness values are tuned for 60 steps per second and scale with the step.
    struct Options {
        float radius = 4.0F;
        float smoothingRadius = 16.0F;
        float density = 1.0F;
        float friction = 0.0F;
        float restitution = 0.0F;
        float restDensity = 2.0F;
        float stiffness = 0.01F;
        float nearStiffness = 0.02F;
        float viscosity = 0.2F;
        std::size_t maxParticles = 4096;
        CollisionFilter filter{};
    };

    // Throws std::invalid_argument when the radius is not positive, the smoothing radius is not larger than it or the material is invalid.
    Fluid(World& owner, const Options& settings);
    ~Fluid();

    Fluid(const Fluid&) = delete;
    Fluid& operator=(const Fluid&) = delete;

    // Adds a particle and returns false when the fluid is full.
    bool spawn(math::Vec2 position, math::Vec2 velocity = {});
    // Fills the area with particles two radii apart and returns how many it added.
    std::size_t fill(const math::Rect& area, math::Vec2 velocity = {});
    // Removes a particle, moving the last particle into its place. Throws std::out_of_range for an unknown index.
    void remove(std::size_t index);
    void clear();

    // Applies the fluid forces for the next world step, so call it right before World::step with the same time.
    void update(float deltaSeconds);

    [[nodiscard]] std::size_t size();
    [[nodiscard]] const std::vector<Body>& getBodies();
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }

  private:
    struct Pair {
        std::uint32_t first = 0;
        std::uint32_t second = 0;
        float weight = 0.0F;
        math::Vec2 direction{};
    };

    [[nodiscard]] static std::uint64_t cellKey(int column, int row) noexcept;
    void dropDestroyed();
    void findPairs();

    World& world;
    Options options;
    Shape::Options particle;
    std::vector<Body> bodies;
    std::vector<math::Vec2> positions;
    std::vector<math::Vec2> velocities;
    std::vector<float> densities;
    std::vector<float> nearDensities;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> cells;
    std::vector<Pair> pairs;
};

} // namespace haylen::physics2d
