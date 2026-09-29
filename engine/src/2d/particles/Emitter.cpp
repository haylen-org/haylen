#include "haylen/2d/particles/Emitter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::particles2d {

void Emitter::validate(const EmitterConfig& settings) {
    if (!settings.texture.isValid() || settings.maxParticles == 0 || settings.colors.empty()) {
        throw std::invalid_argument("A particle emitter needs a texture, room for particles and at least one color.");
    }
    if (settings.rate < 0.0F || settings.lifetime.min <= 0.0F || settings.lifetime.max < settings.lifetime.min) {
        throw std::invalid_argument("Particles need a non-negative rate and a positive lifetime range.");
    }
    if (settings.duration < 0.0F || settings.prewarm < 0.0F || (settings.loop && settings.duration <= 0.0F)) {
        throw std::invalid_argument("Particle durations and prewarm times cannot be negative, and a looping emitter needs a duration.");
    }
    for (const EmitterConfig::Burst& planned : settings.bursts) {
        if (planned.time < 0.0F || (settings.duration > 0.0F && planned.time > settings.duration)) {
            throw std::invalid_argument("Particle bursts need a time inside the emission cycle.");
        }
    }
}

EmitterConfig Emitter::prepared(EmitterConfig settings) {
    validate(settings);
    std::stable_sort(settings.bursts.begin(), settings.bursts.end(), [](const EmitterConfig::Burst& lhs, const EmitterConfig::Burst& rhs) { return lhs.time < rhs.time; });
    return settings;
}

template <typename Visit> void Emitter::Particles::forEachArray(Visit&& visit) {
    visit(positions);
    visit(velocities);
    visit(ages);
    visit(lifetimes);
    visit(startSizes);
    visit(endSizes);
    visit(rotations);
    visit(spins);
    visit(radial);
    visit(tangential);
}

// Compacts the arrays in place and keeps the order particles draw in.
debug::ObjectCounter Emitter::emitterCounter("ParticleEmitter", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter Emitter::particleCounter("Particle", debug::ObjectCounter::Kind::Native);

void Emitter::Particles::removeExpired() {
    std::size_t kept = 0;
    for (std::size_t index = 0; index < ages.size(); ++index) {
        if (ages[index] >= lifetimes[index]) {
            continue;
        }
        if (kept != index) {
            forEachArray([kept, index](auto& values) { values[kept] = values[index]; });
        }
        ++kept;
    }
    forEachArray([kept](auto& values) { values.resize(kept); });
}

Emitter::Emitter(EmitterConfig settings, std::uint64_t seed) : config(prepared(std::move(settings))), random(seed) {
    particles.forEachArray([capacity = config.maxParticles](auto& values) { values.reserve(capacity); });
    restart();
}

void Emitter::setConfig(EmitterConfig value) {
    config = prepared(std::move(value));
    nextBurst = std::min(nextBurst, config.bursts.size());
}

void Emitter::restart() {
    clear();
    time = 0.0F;
    nextBurst = 0;
    prewarmed = false;
    emitting = true;
}

void Emitter::prewarm() {
    if (prewarmed) {
        return;
    }
    prewarmed = true;
    for (float elapsed = 0.0F; elapsed < config.prewarm; elapsed += kPrewarmStep) {
        step(std::min(kPrewarmStep, config.prewarm - elapsed), nullptr);
    }
}

float Emitter::pick(math::FloatRange range) {
    return range.min == range.max ? range.min : random.range(range.min, range.max);
}

void Emitter::spawn() {
    if (getCount() >= config.maxParticles) {
        return;
    }

    float angle = config.direction + random.range(-config.spread, config.spread) * 0.5F;
    math::Vec2 offset{};
    switch (config.shape) {
    case EmitterConfig::Shape::Point:
        break;
    case EmitterConfig::Shape::Circle: {
        // The square root keeps the density even across the disc.
        const float radius = config.shapeSize.x * std::sqrt(random.nextFloat());
        const float around = random.range(0.0F, math::Math::kTau);
        offset = math::Vec2{std::cos(around), std::sin(around)} * radius;
        break;
    }
    case EmitterConfig::Shape::Ring: {
        const float around = random.range(0.0F, math::Math::kTau);
        offset = math::Vec2{std::cos(around), std::sin(around)} * config.shapeSize.x;
        break;
    }
    case EmitterConfig::Shape::Rectangle:
        offset = {random.range(-config.shapeSize.x, config.shapeSize.x), random.range(-config.shapeSize.y, config.shapeSize.y)};
        break;
    case EmitterConfig::Shape::Cone:
        offset = math::Vec2{std::cos(angle), std::sin(angle)} * (config.shapeSize.x * std::sqrt(random.nextFloat()));
        break;
    }

    const float speed = pick(config.speed);
    particles.positions.push_back(config.localSpace ? offset : position + offset);
    particles.velocities.push_back(math::Vec2{std::cos(angle), std::sin(angle)} * speed);
    particles.ages.push_back(0.0F);
    particles.lifetimes.push_back(pick(config.lifetime));
    particles.startSizes.push_back(pick(config.startSize));
    particles.endSizes.push_back(pick(config.endSize));
    particles.rotations.push_back(0.0F);
    particles.spins.push_back(pick(config.spin));
    particles.radial.push_back(pick(config.radialAcceleration));
    particles.tangential.push_back(pick(config.tangentialAcceleration));
}

void Emitter::burst(std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        spawn();
    }
    liveParticles.set(getCount());
}

// Radial acceleration pushes away from the emitter and tangential acceleration turns around it.
void Emitter::simulate(float deltaSeconds, std::size_t begin, std::size_t end) noexcept {
    const float damping = std::exp(-config.damping * deltaSeconds);
    const math::Vec2 center = config.localSpace ? math::Vec2{} : position;
    for (std::size_t index = begin; index < end; ++index) {
        math::Vec2 acceleration = config.gravity;
        if (particles.radial[index] != 0.0F || particles.tangential[index] != 0.0F) {
            const math::Vec2 outward = (particles.positions[index] - center).getNormalized();
            acceleration += outward * particles.radial[index] + math::Vec2{-outward.y, outward.x} * particles.tangential[index];
        }
        particles.ages[index] += deltaSeconds;
        particles.velocities[index] = (particles.velocities[index] + acceleration * deltaSeconds) * damping;
        particles.positions[index] += particles.velocities[index] * deltaSeconds;
        particles.rotations[index] += particles.spins[index] * deltaSeconds;
    }
}

void Emitter::update(float deltaSeconds) {
    prewarm();
    step(deltaSeconds, nullptr);
}

void Emitter::update(float deltaSeconds, core::JobSystem& jobs) {
    prewarm();
    step(deltaSeconds, &jobs);
}

void Emitter::step(float deltaSeconds, core::JobSystem* jobs) {
    if (jobs == nullptr) {
        simulate(deltaSeconds, 0, getCount());
    } else {
        // clang-format off
        jobs->parallelFor(0, getCount(), kParallelGrain, [this, deltaSeconds](std::size_t begin, std::size_t end) {
            simulate(deltaSeconds, begin, end);
        });
        // clang-format on
    }
    particles.removeExpired();
    emit(deltaSeconds);
    liveParticles.set(getCount());
}

// Walks the emission cycle in steps that end at the cycle boundary, so bursts and the rate follow the cycle even when one frame spans several loops.
void Emitter::emit(float deltaSeconds) {
    if (!emitting) {
        emitDebt = 0.0F;
        return;
    }

    const float duration = config.duration;
    float remaining = deltaSeconds;
    while (emitting && remaining > 0.0F) {
        if (duration > 0.0F && time >= duration) {
            time = 0.0F;
            nextBurst = 0;
        }
        const bool finishesCycle = duration > 0.0F && remaining >= duration - time;
        const float advance = finishesCycle ? duration - time : remaining;
        time = finishesCycle ? duration : time + advance;
        remaining -= advance;

        while (nextBurst < config.bursts.size() && config.bursts[nextBurst].time <= time) {
            burst(config.bursts[nextBurst++].count);
        }
        // Fractional particles carry over, so low rates still emit on average at the requested pace.
        emitDebt += config.rate * advance;
        while (emitDebt >= 1.0F) {
            emitDebt -= 1.0F;
            spawn();
        }
        if (finishesCycle && !config.loop) {
            emitting = false;
        }
    }
}

math::Color Emitter::colorAt(float life) const noexcept {
    const std::vector<math::Color>& colors = config.colors;
    if (colors.size() == 1) {
        return colors.front();
    }
    const float scaled = math::Math::saturate(life) * static_cast<float>(colors.size() - 1);
    const auto index = std::min(static_cast<std::size_t>(scaled), colors.size() - 2);
    return math::Color::lerp(colors[index], colors[index + 1], scaled - static_cast<float>(index));
}

void Emitter::draw(graphics2d::Renderer& renderer) const {
    if (getCount() == 0) {
        return;
    }

    std::vector<graphics2d::SpriteInstance> instances;
    instances.reserve(getCount());
    const math::Vec2 origin = config.localSpace ? position : math::Vec2{};
    for (std::size_t index = 0; index < getCount(); ++index) {
        const float life = particles.ages[index] / particles.lifetimes[index];
        const float size = math::Math::lerp(particles.startSizes[index], particles.endSizes[index], life);
        math::Rect source{};
        if (!config.frames.empty()) {
            const auto frame = std::min(static_cast<std::size_t>(life * static_cast<float>(config.frames.size())), config.frames.size() - 1);
            source = config.frames[frame];
        }
        instances.push_back({.position = origin + particles.positions[index], .size = {size, size}, .source = source, .rotation = particles.rotations[index], .color = colorAt(life)});
    }
    renderer.drawBatch(config.texture, instances, config.order);
}

void Emitter::clear() noexcept {
    particles.forEachArray([](auto& values) { values.clear(); });
    emitDebt = 0.0F;
    liveParticles.set(0);
}

} // namespace haylen::particles2d
