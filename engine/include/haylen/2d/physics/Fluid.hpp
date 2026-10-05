#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// A liquid of particles kept in flat arrays outside Box2D and simulated after every step of its world. The particles hold together and spread apart with the double density relaxation of particle fluids over a spatial hash, collide with the shapes of the world through queries, and push the dynamic bodies they hit, so crates float and sink by their density. The passes run in parallel on the job system of the world when it has one. The fluid must go before its world.
class Fluid final {
  public:
    // Particles interact within the smoothing radius and collide with shapes at their radius. Pressure pulls them toward the rest density, near pressure keeps them from clumping and viscosity evens out the speeds of neighbors. Stiffness values are tuned for 60 steps per second and scale with the step. The density, in kilograms per square meter of the area of a particle, gives the particles the mass they push bodies with. Friction slows particles that slide along shapes, and restitution bounces them off. Speeds are capped at `maxSpeed` world units per second.
    struct Options {
        float radius = 4.0F;
        float smoothingRadius = 16.0F;
        float density = 1.0F;
        float friction = 0.1F;
        float restitution = 0.0F;
        float restDensity = 1.8F;
        float stiffness = 0.008F;
        float nearStiffness = 0.02F;
        float viscosity = 0.15F;
        float gravityScale = 1.0F;
        float maxSpeed = 3000.0F;
        std::size_t maxParticles = 8192;
        CollisionFilter filter{};
    };

    // Throws `std::invalid_argument` when the radius is not positive, the smoothing radius is not larger than it, or a density, stiffness, viscosity, friction, restitution or speed is negative.
    Fluid(World& owner, const Options& settings);
    ~Fluid();

    Fluid(const Fluid&) = delete;
    Fluid& operator=(const Fluid&) = delete;

    // Adds a particle and returns `false` when the fluid is full.
    bool spawn(math::Vec2 position, math::Vec2 velocity = {});
    // Fills the area with particles half a smoothing radius apart, nudged off a perfect grid, and returns how many it added.
    std::size_t fill(const math::Rect& area, math::Vec2 velocity = {});
    // Removes a particle, moving the last particle into its place. Throws `std::out_of_range` for an unknown index.
    void remove(std::size_t index);
    void clear();

    [[nodiscard]] std::size_t size() const noexcept {
        return positions.size();
    }
    [[nodiscard]] std::span<const math::Vec2> getPositions() const noexcept {
        return positions;
    }
    [[nodiscard]] std::span<const math::Vec2> getVelocities() const noexcept {
        return velocities;
    }
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    // How long the last fluid pass took, in milliseconds.
    [[nodiscard]] float getStepMilliseconds() const noexcept {
        return stepMilliseconds;
    }

  private:
    struct Neighbor {
        std::uint32_t index = 0;
        float weight = 0.0F;
        math::Vec2 direction{};
    };

    class Collider;

    // A particle keeps at most this many neighbors, which only a fluid squeezed far past its rest density reaches.
    static constexpr std::size_t kMaxNeighbors = 40;
    // Particles go to the workers in chunks of at least this many.
    static constexpr std::size_t kGrain = 256;

    void step(float deltaSeconds);
    void buildGrid();
    void findNeighbors(std::size_t begin, std::size_t end);
    void relax(std::size_t begin, std::size_t end, float frames);
    void smooth(std::size_t begin, std::size_t end, float frames);
    [[nodiscard]] std::uint32_t cellOf(int column, int row) const noexcept;
    template <typename Work> void forEachRange(Work&& work);

    World& world;
    Options options;
    float particleMass = 0.0F;
    float stepMilliseconds = 0.0F;
    std::uint64_t hook = 0;
    std::vector<math::Vec2> positions;
    std::vector<math::Vec2> previous;
    std::vector<math::Vec2> velocities;
    std::vector<math::Vec2> scratch;
    std::vector<float> densities;
    std::vector<float> nearDensities;
    std::vector<Neighbor> neighbors;
    std::vector<std::uint8_t> neighborCounts;
    std::vector<std::uint32_t> cellKeys;
    std::vector<std::uint32_t> cellStarts;
    std::vector<std::uint32_t> cellCursors;
    std::vector<std::uint32_t> sorted;
    std::uint32_t cellMask = 0;
};

} // namespace haylen::physics2d
