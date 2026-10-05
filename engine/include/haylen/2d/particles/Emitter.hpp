#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/RayBatch.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedCount.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {
class World;
}

namespace haylen::particles2d {

// Simulates particles on the frame thread and draws them in one batch. Particles live in parallel arrays, so large emitters update in parallel chunks when given the job system, and the features a configuration leaves out cost nothing. The emitters of the sub-emitters update and draw with their parent.
class Emitter final {
  public:
    // A live particle as it moves and draws now: its place in the world, its velocity, its age and lifetime, and the sprite it draws as.
    struct Particle {
        math::Vec2 position{};
        math::Vec2 velocity{};
        float age = 0.0F;
        float lifetime = 0.0F;
        graphics2d::SpriteInstance sprite;
    };

    explicit Emitter(EmitterConfig settings, std::uint64_t seed = 0);

    Emitter(const Emitter&) = delete;
    Emitter& operator=(const Emitter&) = delete;
    Emitter(Emitter&&) noexcept = default;
    Emitter& operator=(Emitter&&) noexcept = default;
    ~Emitter() = default;

    void burst(std::size_t count);
    void update(float deltaSeconds);
    void update(float deltaSeconds, core::JobSystem& jobs);
    void draw(graphics2d::Renderer& renderer) const;
    void clear() noexcept;

    // Clears every particle and starts the emission cycle again after its delay. The prewarm time runs on the next update, from the emitter position at that moment.
    void restart();

    [[nodiscard]] std::size_t getCount() const noexcept {
        return particles.positions.size();
    }

    // Live particle positions are in world space, or relative to the emitter position for local space emitters.
    [[nodiscard]] std::span<const math::Vec2> getPositions() const noexcept {
        return particles.positions;
    }

    // Returns the live particle at an index from 0 to the count, in the order particles were born.
    [[nodiscard]] Particle getParticle(std::size_t index) const;

    // An emitter is alive while it emits, while it waits for its delay, or while it or its sub-emitters have particles.
    [[nodiscard]] bool isAlive() const noexcept;
    [[nodiscard]] float getCycleTime() const noexcept {
        return time;
    }
    [[nodiscard]] const EmitterConfig& getConfig() const noexcept {
        return config;
    }
    void setConfig(EmitterConfig value);

    // Moves an attractor of the configuration, in the space it was given in.
    void setAttractor(std::size_t index, math::Vec2 value);

    // Replaces the points of the polygon and polyline shapes, such as the path of a bolt that changes every frame.
    void setShapePoints(std::vector<math::Vec2> points);

    // Gives world collision the shapes of a physics world that the filter lets through, or none. The world must outlive the emitter or be removed from it first.
    void setCollisionWorld(const physics2d::World* world, const physics2d::CollisionFilter& filter = {});

    // The position, scale and rotation apply to particles as they are born: the scale multiplies the spawn area, speeds, sizes, gravity and accelerations, and the rotation turns the direction, the spawn area and the particles.
    math::Vec2 position{};
    float scale = 1.0F;
    float rotation = 0.0F;
    bool emitting = true;

  private:
    struct Particles {
        std::vector<math::Vec2> positions;
        std::vector<math::Vec2> velocities;
        std::vector<float> ages;
        std::vector<float> lifetimes;
        std::vector<float> startSizes;
        std::vector<float> endSizes;
        std::vector<float> rotations;
        std::vector<float> spins;
        std::vector<float> radial;
        std::vector<float> tangential;

        // The arrays of optional features stay empty when the configuration leaves the feature out.
        std::vector<math::Color> tints;
        std::vector<float> aspects;
        std::vector<std::uint32_t> frames;
        std::vector<std::uint8_t> flags;

        // The trail of each particle takes a block of points, the newest first.
        std::vector<math::Vec2> trails;
        std::vector<std::uint16_t> trailCounts;

        template <typename Visit> void forEachArray(Visit&& visit);
    };

    // One firing of a burst, which repeats every interval for its cycles.
    struct Firing {
        float time = 0.0F;
        std::size_t burst = 0;
    };

    // Where a new particle starts relative to the emitter, the angle it leaves at and, for image shapes, the pixel it took.
    struct Spawn {
        math::Vec2 offset{};
        float angle = 0.0F;
        std::size_t pixel = 0;
    };

    // A particle that hit something stops when it sticks, and reports the hit to the collision sub-emitters of its step.
    static constexpr std::uint8_t kStuck = 1;
    static constexpr std::uint8_t kHit = 2;

    // Hits slower than this only stop a particle against the surface, so particles that rest on a floor neither bounce nor report hits.
    static constexpr float kRestingSpeed = 30.0F;
    static constexpr float kPrewarmStep = 1.0F / 30.0F;
    static constexpr float kMaxPrewarm = 60.0F;
    static constexpr float kMinimumCycle = 0.001F;
    static constexpr std::size_t kMaxParticles = 1000000;
    static constexpr std::size_t kMaxTrailLength = 64;
    static constexpr std::size_t kMaxTrailPoints = 1000000;
    static constexpr std::size_t kMaxParticleLights = 1024;
    static constexpr std::size_t kParallelGrain = 4096;
    static debug::ObjectCounter& emitterCounter;
    static debug::ObjectCounter& particleCounter;

    static void validate(const EmitterConfig& settings);
    static void validateBursts(const EmitterConfig& settings);
    static void validateMotion(const EmitterConfig& settings);
    static void validateLook(const EmitterConfig& settings);
    static void validateShape(const EmitterConfig& settings);

    // Derives what the configuration implies: the frames, the firings of the bursts, the weights of the spawn area, the arrays of optional features and the emitters of the sub-emitters.
    void prepare();
    void prepareShape();
    void prepareSubEmitters(const std::vector<EmitterConfig::SubEmitter>& previous);

    // Spawns up to `count` particles, as many as `maxParticles` leaves room for, spread along the segment the emitter moved this step, with the velocity they inherit and a tint over their colors.
    void spawn(std::size_t count, math::Vec2 from, math::Vec2 to, math::Vec2 inherited, math::Color tint);
    void spawnParticle(math::Vec2 origin, math::Vec2 inherited, math::Color tint);
    [[nodiscard]] Spawn pickSpawn();
    [[nodiscard]] math::Vec2 pickShapePoint(std::size_t& pixel);
    [[nodiscard]] math::Color pickTint(std::size_t pixel);

    void prewarm();
    void step(float deltaSeconds, core::JobSystem* jobs);
    void emit(float deltaSeconds);
    void simulate(float deltaSeconds, std::size_t begin, std::size_t end) noexcept;
    void collide(std::size_t index, math::Vec2 contact, math::Vec2 normal) noexcept;
    void collideWithWorld(core::JobSystem* jobs);
    void removeExpired();
    void runTriggers(float deltaSeconds);
    void trigger(std::size_t subEmitter, std::size_t index);
    void sampleTrails(float deltaSeconds);

    // Returns how many particles the bursts that the cycle time reached since the last call spawn.
    [[nodiscard]] double takeBursts();
    [[nodiscard]] float pick(math::FloatRange range);
    [[nodiscard]] std::size_t pick(EmitterConfig::CountRange range);
    [[nodiscard]] math::Color colorAt(float life) const noexcept;
    [[nodiscard]] math::Color particleColor(std::size_t index) const noexcept;
    [[nodiscard]] math::Rect frameAt(std::size_t index, float life) const noexcept;
    [[nodiscard]] graphics2d::SpriteInstance spriteAt(std::size_t index) const noexcept;
    [[nodiscard]] math::Vec2 getVelocity(std::size_t index) const noexcept;
    [[nodiscard]] static float easeDown(const std::optional<math::EasingCurve>& curve, float life) noexcept;

    // The emitter position in the space of the particles, which is the origin for local space emitters.
    [[nodiscard]] math::Vec2 getCenter() const noexcept;

    // The point of the world that particle positions are relative to, the emitter position for local space emitters.
    [[nodiscard]] math::Vec2 getOrigin() const noexcept;

    void drawTrails(graphics2d::Renderer& renderer) const;
    void drawLights(graphics2d::Renderer& renderer) const;

    EmitterConfig config;
    std::uint64_t seed = 0;
    math::Random random;
    math::Noise2D noise;
    Particles particles;
    std::vector<math::Rect> frames;
    std::vector<Firing> firings;
    std::vector<float> shapeWeights;
    std::vector<std::unique_ptr<Emitter>> children;
    mutable std::vector<graphics2d::SpriteInstance> instances;
    mutable std::vector<math::Vec2> trailLine;
    mutable std::vector<float> trailPlaces;
    mutable std::vector<graphics2d::MeshVertex> trailVertices;
    mutable std::vector<std::uint32_t> trailIndices;

    // The positions before the step, which world collision casts rays from.
    std::vector<math::Vec2> previousPositions;
    physics2d::RayBatch rays;
    const physics2d::World* collisionWorld = nullptr;
    physics2d::CollisionFilter collisionFilter{};

    bool usesTints = false;
    bool usesAspects = false;
    bool usesFrameStarts = false;
    bool usesHits = false;

    // The emitter of a sub-emitter that inherits the color of its parent tints every particle it spawns.
    bool inheritsColor = false;
    float emitDebt = 0.0F;
    float distanceDebt = 0.0F;
    float time = 0.0F;
    float delayLeft = 0.0F;
    float clock = 0.0F;
    float trailClock = 0.0F;
    std::size_t nextBurst = 0;
    std::size_t spawned = 0;
    std::size_t peakCount = 0;
    math::Vec2 previousPosition{};
    math::Vec2 velocity{};
    bool prewarmed = false;
    debug::TrackedObject tracked{emitterCounter};
    debug::TrackedCount liveParticles{particleCounter};
};

} // namespace haylen::particles2d
