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
    if (settings.maxParticles > kMaxParticles) {
        throw std::invalid_argument("A particle emitter holds at most 1000000 particles.");
    }
    if (!(settings.rate >= 0.0F && std::isfinite(settings.rate)) || !(settings.lifetime.min > 0.0F && settings.lifetime.max >= settings.lifetime.min)) {
        throw std::invalid_argument("Particles need a finite, non-negative rate and a positive lifetime range.");
    }
    if (!(settings.duration >= 0.0F && std::isfinite(settings.duration)) || !(settings.prewarm >= 0.0F && settings.prewarm <= kMaxPrewarm)) {
        throw std::invalid_argument("Particle durations must be finite and not negative, and prewarm times must be from 0 to 60 seconds.");
    }
    if (settings.loop && settings.duration < kMinimumCycle) {
        throw std::invalid_argument("A looping particle emitter needs a duration of at least 0.001 seconds.");
    }
    for (const EmitterConfig::Burst& planned : settings.bursts) {
        if (!(planned.time >= 0.0F && std::isfinite(planned.time)) || (settings.duration > 0.0F && planned.time > settings.duration)) {
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

debug::ObjectCounter& Emitter::emitterCounter = *new debug::ObjectCounter("ParticleEmitter", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter& Emitter::particleCounter = *new debug::ObjectCounter("Particle", debug::ObjectCounter::Kind::Native);

// Compacts the arrays in place and keeps the order particles draw in.
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

void Emitter::spawn(std::size_t count) {
    const std::size_t room = config.maxParticles - std::min(getCount(), config.maxParticles);
    for (std::size_t index = std::min(count, room); index > 0; --index) {
        spawnParticle();
    }
}

void Emitter::spawnParticle() {
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
    spawn(count);
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

double Emitter::takeBursts() noexcept {
    double count = 0.0;
    while (nextBurst < config.bursts.size() && config.bursts[nextBurst].time <= time) {
        count += static_cast<double>(config.bursts[nextBurst++].count);
    }
    return count;
}

// Follows the emission cycle to the end of the frame, so bursts and the rate keep their pace even when one frame spans several loops. The whole loops inside a long frame count at once.
void Emitter::emit(float deltaSeconds) {
    if (!emitting) {
        emitDebt = 0.0F;
        return;
    }
    if (!(deltaSeconds > 0.0F)) {
        return;
    }

    const float duration = config.duration;
    float emitted = deltaSeconds;
    double due = 0.0;
    if (duration > 0.0F && time >= duration) {
        time = 0.0F;
        nextBurst = 0;
    }
    if (duration <= 0.0F || deltaSeconds < duration - time) {
        time += deltaSeconds;
        due += takeBursts();
    } else {
        const float left = duration - time;
        time = duration;
        due += takeBursts();
        if (!config.loop) {
            emitting = false;
            emitted = left;
        } else {
            // Every whole loop fires all the bursts, and the last partial loop fires the bursts it reaches.
            const float rest = deltaSeconds - left;
            const float partial = std::fmod(rest, duration);
            double everyBurst = 0.0;
            for (const EmitterConfig::Burst& planned : config.bursts) {
                everyBurst += static_cast<double>(planned.count);
            }
            due += everyBurst * static_cast<double>(std::round((rest - partial) / duration));
            if (partial > 0.0F) {
                time = partial;
                nextBurst = 0;
                due += takeBursts();
            }
        }
    }

    // Fractional particles carry over, so low rates still emit on average at the requested pace.
    emitDebt += config.rate * emitted;
    const float whole = std::floor(emitDebt);
    emitDebt -= whole;
    due += static_cast<double>(whole);
    spawn(due < static_cast<double>(config.maxParticles) ? static_cast<std::size_t>(due) : config.maxParticles);
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

    instances.clear();
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
