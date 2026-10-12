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

// A liquid of particles kept in flat arrays outside Box2D and simulated after every step of its world. The particles keep the density of particles half a smoothing radius apart with the position based density constraints of particle fluids over a spatial hash, so the liquid barely compresses under its own weight, collide with the shapes of the world through queries along the path of each step, and push the dynamic bodies they hit, so crates float and sink by their density. The passes run in parallel on the job system of the world when it has one. The fluid must go before its world.
class Fluid final {
  public:
    // Particles interact within the smoothing radius and collide with shapes at their radius. Each step pushes compressed particles apart in `iterations` passes, where more passes compress the liquid less. Viscosity, from 0 to 1, evens out the speeds of neighbors. The density, in kilograms per square meter of the area of a particle, gives the particles the mass they push bodies with. Friction slows particles that slide along shapes, and restitution bounces them off. Speeds are capped at `maxSpeed` world units per second.
    struct Options {
        float radius = 4.0F;
        float smoothingRadius = 16.0F;
        float density = 1.0F;
        float friction = 0.1F;
        float restitution = 0.0F;
        float viscosity = 0.1F;
        int iterations = 5;
        float gravityScale = 1.0F;
        float maxSpeed = 3000.0F;
        std::size_t maxParticles = 8192;
        CollisionFilter filter{};
    };

    // Throws `std::invalid_argument` when the radius is not positive, the smoothing radius is not larger than it, there is no iteration, the viscosity is not between 0 and 1, or a density, friction, restitution or speed is negative.
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
    class Collider;

    // A particle keeps at most this many neighbors, which only a liquid squeezed far past its rest density reaches.
    static constexpr std::size_t kMaxNeighbors = 40;
    // Particles go to the workers in chunks of at least this many.
    static constexpr std::size_t kGrain = 256;
    // Every particle takes its pushes from all its neighbors at once, which overshoots where many push the same way, so a pass moves the particles by this share of their pushes.
    static constexpr float kRelaxation = 0.5F;
    // The density passes move a particle at most this many meters per second, like the push of Box2D contacts, so an impact squeezes the liquid for a moment instead of throwing its particles apart.
    static constexpr float kPushSpeed = 3.0F;
    // A push that grows as two particles come closer than `kClumpDistance` smoothing radii keeps them from collapsing onto one point, such as in the corners of a container.
    static constexpr float kClumpStrength = 0.1F;
    static constexpr float kClumpDistance = 0.3F;

    // The kernel of the density and its slope, for distances in smoothing radii.
    [[nodiscard]] static float kernel(float distance) noexcept;
    [[nodiscard]] static float slope(float distance) noexcept;

    void step(float deltaSeconds);
    void buildGrid();
    void findNeighbors(std::size_t begin, std::size_t end);
    void solveDensity(std::size_t begin, std::size_t end);
    void separate(std::size_t begin, std::size_t end, float limit);
    void smooth(std::size_t begin, std::size_t end);
    [[nodiscard]] std::uint32_t cellOf(int column, int row) const noexcept;
    template <typename Work> void forEachRange(Work&& work);

    World& world;
    Options options;
    float particleMass = 0.0F;
    float restDensity = 1.0F;
    float clumpKernel = 1.0F;
    float stepMilliseconds = 0.0F;
    std::uint64_t hook = 0;
    std::vector<math::Vec2> positions;
    std::vector<math::Vec2> previous;
    std::vector<math::Vec2> velocities;
    std::vector<math::Vec2> scratch;
    std::vector<float> lambdas;
    std::vector<std::uint32_t> neighbors;
    std::vector<std::uint8_t> neighborCounts;
    std::vector<std::uint32_t> cellKeys;
    std::vector<std::uint32_t> cellStarts;
    std::vector<std::uint32_t> cellCursors;
    std::vector<std::uint32_t> sorted;
    std::uint32_t cellMask = 0;
};

} // namespace haylen::physics2d
