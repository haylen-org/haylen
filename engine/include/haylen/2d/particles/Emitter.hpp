#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedCount.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::particles2d {

// Simulates particles on the frame thread and draws them in one batch. Particles live in parallel arrays, so large emitters update in parallel chunks when given the job system.
class Emitter final {
  public:
    explicit Emitter(EmitterConfig settings, std::uint64_t seed = 0);

    void burst(std::size_t count);
    void update(float deltaSeconds);
    void update(float deltaSeconds, core::JobSystem& jobs);
    void draw(graphics2d::Renderer& renderer) const;
    void clear() noexcept;

    // Clears every particle and starts the emission cycle again. The prewarm time runs on the next update, from the emitter position at that moment.
    void restart();

    [[nodiscard]] std::size_t getCount() const noexcept {
        return particles.positions.size();
    }

    // Live particle positions are in world space, or relative to the emitter position for local space emitters.
    [[nodiscard]] std::span<const math::Vec2> getPositions() const noexcept {
        return particles.positions;
    }

    [[nodiscard]] bool isAlive() const noexcept {
        return emitting || getCount() > 0;
    }
    [[nodiscard]] float getCycleTime() const noexcept {
        return time;
    }
    [[nodiscard]] const EmitterConfig& getConfig() const noexcept {
        return config;
    }
    void setConfig(EmitterConfig value);

    math::Vec2 position{};
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

        template <typename Visit> void forEachArray(Visit&& visit);
        void removeExpired();
    };

    static constexpr float kPrewarmStep = 1.0F / 30.0F;
    static constexpr float kMaxPrewarm = 60.0F;
    static constexpr float kMinimumCycle = 0.001F;
    static constexpr std::size_t kMaxParticles = 1000000;
    static debug::ObjectCounter& emitterCounter;
    static debug::ObjectCounter& particleCounter;
    static constexpr std::size_t kParallelGrain = 4096;

    static void validate(const EmitterConfig& settings);
    [[nodiscard]] static EmitterConfig prepared(EmitterConfig settings);

    // Spawns up to count particles, as many as maxParticles leaves room for.
    void spawn(std::size_t count);
    void spawnParticle();
    void prewarm();
    void step(float deltaSeconds, core::JobSystem* jobs);
    void emit(float deltaSeconds);
    void simulate(float deltaSeconds, std::size_t begin, std::size_t end) noexcept;

    // Returns how many particles the bursts that the cycle time reached since the last call spawn.
    [[nodiscard]] double takeBursts() noexcept;
    [[nodiscard]] float pick(math::FloatRange range);
    [[nodiscard]] math::Color colorAt(float life) const noexcept;

    EmitterConfig config;
    math::Random random;
    Particles particles;
    mutable std::vector<graphics2d::SpriteInstance> instances;
    float emitDebt = 0.0F;
    float time = 0.0F;
    std::size_t nextBurst = 0;
    bool prewarmed = false;
    debug::TrackedObject tracked{emitterCounter};
    debug::TrackedCount liveParticles{particleCounter};
};

} // namespace haylen::particles2d
