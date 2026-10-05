#include "haylen/2d/particles/Emitter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/LightFlicker.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/2d/particles/Ribbon.hpp"
#include "haylen/2d/physics/Raycaster.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::particles2d {

debug::ObjectCounter& Emitter::emitterCounter = *new debug::ObjectCounter("ParticleEmitter", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter& Emitter::particleCounter = *new debug::ObjectCounter("Particle", debug::ObjectCounter::Kind::Native);

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
    if (!(settings.delay >= 0.0F && std::isfinite(settings.delay)) || !(settings.rateOverDistance >= 0.0F && std::isfinite(settings.rateOverDistance))) {
        throw std::invalid_argument("A particle delay and a rate over distance must be finite and not negative.");
    }
    validateBursts(settings);
    validateMotion(settings);
    validateLook(settings);
    validateShape(settings);
}

void Emitter::validateBursts(const EmitterConfig& settings) {
    for (const EmitterConfig::Burst& planned : settings.bursts) {
        if (planned.cycles < 1 || !(planned.interval >= 0.0F && std::isfinite(planned.interval)) || planned.count.min > planned.count.max || !(planned.probability >= 0.0F && planned.probability <= 1.0F)) {
            throw std::invalid_argument("A particle burst needs at least one cycle, an interval of at least 0, a count range whose minimum is at most its maximum and a probability from 0 to 1.");
        }
        const float last = planned.time + planned.interval * static_cast<float>(planned.cycles - 1);
        if (!(planned.time >= 0.0F && std::isfinite(last)) || (settings.duration > 0.0F && last > settings.duration)) {
            throw std::invalid_argument("Particle bursts need a time inside the emission cycle.");
        }
    }
}

void Emitter::validateMotion(const EmitterConfig& settings) {
    const EmitterConfig::Turbulence& turbulence = settings.turbulence;
    if (!std::isfinite(turbulence.strength) || !(turbulence.frequency > 0.0F && std::isfinite(turbulence.frequency)) || !std::isfinite(turbulence.speed)) {
        throw std::invalid_argument("Particle turbulence needs a finite strength and speed and a positive frequency.");
    }
    for (const EmitterConfig::Attractor& attractor : settings.attractors) {
        if (!std::isfinite(attractor.strength) || !(attractor.radius >= 0.0F && std::isfinite(attractor.radius)) || !(attractor.killRadius >= 0.0F && std::isfinite(attractor.killRadius))) {
            throw std::invalid_argument("A particle attractor needs a finite strength and a radius and a kill radius of at least 0.");
        }
    }
    const EmitterConfig::Collision& collision = settings.collision;
    if (!(collision.bounce >= 0.0F && std::isfinite(collision.bounce)) || !(collision.friction >= 0.0F && collision.friction <= 1.0F) || !(collision.lifeLoss >= 0.0F && collision.lifeLoss <= 1.0F)) {
        throw std::invalid_argument("Particle collision needs a bounce of at least 0, and a friction and a life loss from 0 to 1.");
    }
    if (collision.type == EmitterConfig::Collision::Type::Bounds && !(collision.area.width > 0.0F && collision.area.height > 0.0F)) {
        throw std::invalid_argument("Particle collision with bounds needs an area with a positive width and height.");
    }
    if (settings.bounds && !(settings.bounds->width > 0.0F && settings.bounds->height > 0.0F)) {
        throw std::invalid_argument("Particle bounds need a positive width and height.");
    }
    for (const EmitterConfig::SubEmitter& sub : settings.subEmitters) {
        if (!sub.config || sub.count.min > sub.count.max || !(sub.rate >= 0.0F && std::isfinite(sub.rate)) || !(sub.probability >= 0.0F && sub.probability <= 1.0F) || !std::isfinite(sub.inheritVelocity)) {
            throw std::invalid_argument("A particle sub-emitter needs an effect, a count range whose minimum is at most its maximum, a finite rate of at least 0 and a probability from 0 to 1.");
        }
    }
}

void Emitter::validateLook(const EmitterConfig& settings) {
    if (!settings.frames.empty() && (settings.frameGrid.columns > 0 || settings.frameGrid.rows > 0)) {
        throw std::invalid_argument("A particle emitter takes either frames or a frame grid, not both.");
    }
    const EmitterConfig::FrameGrid& grid = settings.frameGrid;
    if ((grid.columns > 0 || grid.rows > 0) && (grid.columns < 1 || grid.rows < 1 || grid.count < 0 || grid.count > grid.columns * grid.rows)) {
        throw std::invalid_argument("A particle frame grid needs at least one column and one row and a count of at most its cells.");
    }
    if (!(settings.frameRate > 0.0F && std::isfinite(settings.frameRate))) {
        throw std::invalid_argument("A particle frame rate must be positive and finite.");
    }
    if (!settings.colorTimes.empty()) {
        const bool inside = std::all_of(settings.colorTimes.begin(), settings.colorTimes.end(), [](float value) { return value >= 0.0F && value <= 1.0F; });
        if (settings.colorTimes.size() != settings.colors.size() || !inside || !std::is_sorted(settings.colorTimes.begin(), settings.colorTimes.end())) {
            throw std::invalid_argument("Particle color times need one time from 0 to 1 for each color, in growing order.");
        }
    }
    const bool scaleValid = !settings.endSizeScale || settings.endSizeScale->min >= 0.0F;
    if (!scaleValid || !(settings.aspect.min > 0.0F) || !(settings.stretch >= 0.0F) || !(settings.rotationStep >= 0.0F) || !(settings.pixelSnap >= 0.0F)) {
        throw std::invalid_argument("A particle end size scale, stretch, rotation step and pixel snap must be at least 0, and an aspect must be positive.");
    }
    const EmitterConfig::ParticleTrail& trail = settings.trail;
    if (trail.length > kMaxTrailLength || trail.length * settings.maxParticles > kMaxTrailPoints || !(trail.lifetime > 0.0F && std::isfinite(trail.lifetime)) || !(trail.widthStart >= 0.0F && trail.widthEnd >= 0.0F) || trail.colors.empty()) {
        throw std::invalid_argument("A particle trail holds at most 64 points, a million points across all the particles of the emitter, and needs a positive lifetime, widths of at least 0 and at least one color.");
    }
    if (settings.light && (!(settings.light->radius > 0.0F) || !(settings.light->intensity >= 0.0F) || !(settings.light->flicker.speed >= 0.0F) || !(settings.light->flicker.amount >= 0.0F && settings.light->flicker.amount <= 1.0F))) {
        throw std::invalid_argument("A particle light needs a positive radius, an intensity and a flicker speed of at least 0 and a flicker amount from 0 to 1.");
    }
    if (settings.particleLights && (!(settings.particleLights->radius > 0.0F) || !(settings.particleLights->intensity >= 0.0F) || settings.particleLights->max > kMaxParticleLights)) {
        throw std::invalid_argument("Particle lights need a positive radius, an intensity of at least 0 and at most 1024 lights.");
    }
}

void Emitter::validateShape(const EmitterConfig& settings) {
    using Shape = EmitterConfig::Shape;
    if (settings.shape == Shape::Polygon && (settings.shapePoints.size() < 3 || std::fabs(math::Geometry::signedArea(settings.shapePoints)) <= 0.0F)) {
        throw std::invalid_argument("The polygon shape of a particle emitter needs at least three points that enclose an area.");
    }
    if (settings.shape == Shape::Polyline && settings.shapePoints.size() < 2) {
        throw std::invalid_argument("The polyline shape of a particle emitter needs at least two points.");
    }
    if (settings.shape == Shape::Image && !settings.shapeImage) {
        throw std::invalid_argument("The image shape of a particle emitter needs a shape image.");
    }
    if (!(settings.shapeThickness >= 0.0F && std::isfinite(settings.shapeThickness))) {
        throw std::invalid_argument("The shape thickness of a particle emitter must be finite and not negative.");
    }
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
    visit(tints);
    visit(aspects);
    visit(frames);
    visit(flags);
    visit(trailCounts);
}

Emitter::Emitter(EmitterConfig settings, std::uint64_t seedValue) : config(std::move(settings)), seed(seedValue), random(seedValue), noise(seedValue) {
    validate(config);
    prepare();
    restart();
}

void Emitter::setConfig(EmitterConfig value) {
    validate(value);
    const std::vector<EmitterConfig::SubEmitter> previous = std::move(config.subEmitters);
    config = std::move(value);
    prepareSubEmitters(previous);
    prepare();
    nextBurst = std::min(nextBurst, firings.size());
}

// The arrays of optional features follow the configuration, so a feature that turns on gives the live particles its neutral value.
void Emitter::prepare() {
    frames = config.frames;
    const EmitterConfig::FrameGrid& grid = config.frameGrid;
    if (grid.columns > 0) {
        const math::Vec2 cell = config.texture.getSize() / math::Vec2{static_cast<float>(grid.columns), static_cast<float>(grid.rows)};
        const int count = grid.count > 0 ? grid.count : grid.columns * grid.rows;
        frames.clear();
        for (int index = 0; index < count; ++index) {
            frames.push_back({static_cast<float>(index % grid.columns) * cell.x, static_cast<float>(index / grid.columns) * cell.y, cell.x, cell.y});
        }
    }

    firings.clear();
    for (std::size_t index = 0; index < config.bursts.size(); ++index) {
        const EmitterConfig::Burst& planned = config.bursts[index];
        for (int cycle = 0; cycle < planned.cycles; ++cycle) {
            firings.push_back({.time = planned.time + planned.interval * static_cast<float>(cycle), .burst = index});
        }
    }
    std::stable_sort(firings.begin(), firings.end(), [](const Firing& lhs, const Firing& rhs) { return lhs.time < rhs.time; });
    prepareShape();

    using Mode = EmitterConfig::FrameMode;
    usesTints = !config.tints.empty() || config.colorFromImage || inheritsColor;
    usesAspects = config.aspect.min != 1.0F || config.aspect.max != 1.0F;
    usesFrameStarts = config.frameMode == Mode::LoopRandomStart || config.frameMode == Mode::Random;
    usesHits = config.collision.type != EmitterConfig::Collision::Type::None;
    const std::size_t count = getCount();
    const std::size_t capacity = config.maxParticles;
    // clang-format off
    const auto follow = [count, capacity](auto& values, bool used, auto neutral) {
        if (!used) {
            values.clear();
            values.shrink_to_fit();
            return;
        }
        if (values.size() != count) {
            values.assign(count, neutral);
        }
        values.reserve(capacity);
    };
    // clang-format on
    follow(particles.positions, true, math::Vec2{});
    follow(particles.velocities, true, math::Vec2{});
    follow(particles.ages, true, 0.0F);
    follow(particles.lifetimes, true, 1.0F);
    follow(particles.startSizes, true, 0.0F);
    follow(particles.endSizes, true, 0.0F);
    follow(particles.rotations, true, 0.0F);
    follow(particles.spins, true, 0.0F);
    follow(particles.radial, true, 0.0F);
    follow(particles.tangential, true, 0.0F);
    follow(particles.tints, usesTints, math::Color::white());
    follow(particles.aspects, usesAspects, 1.0F);
    follow(particles.frames, usesFrameStarts, std::uint32_t{0});
    follow(particles.flags, usesHits, std::uint8_t{0});
    follow(particles.trailCounts, config.trail.length > 0, std::uint16_t{0});
    if (particles.trails.size() != count * config.trail.length) {
        particles.trails.assign(count * config.trail.length, math::Vec2{});
        std::fill(particles.trailCounts.begin(), particles.trailCounts.end(), std::uint16_t{0});
    }
    particles.trails.reserve(capacity * config.trail.length);
}

// Polygons spawn inside their triangles and polylines along their segments, each picked by its share of the area or the length.
void Emitter::prepareShape() {
    shapeWeights.clear();
    const std::vector<math::Vec2>& points = config.shapePoints;
    float total = 0.0F;
    if (config.shape == EmitterConfig::Shape::Polygon) {
        const std::vector<std::uint32_t> triangles = math::Geometry::triangulate(points);
        for (std::size_t index = 0; index + 2 < triangles.size(); index += 3) {
            const math::Vec2 a = points[triangles[index]];
            total += std::fabs(math::Vec2::cross(points[triangles[index + 1]] - a, points[triangles[index + 2]] - a)) * 0.5F;
            shapeWeights.push_back(total);
            shapeWeights.push_back(static_cast<float>(triangles[index]));
            shapeWeights.push_back(static_cast<float>(triangles[index + 1]));
            shapeWeights.push_back(static_cast<float>(triangles[index + 2]));
        }
    } else if (config.shape == EmitterConfig::Shape::Polyline) {
        for (std::size_t index = 0; index + 1 < points.size(); ++index) {
            total += math::Vec2::distance(points[index], points[index + 1]);
            shapeWeights.push_back(total);
        }
    }
}

// Sub-emitters that keep their effect keep their emitter and its live particles.
void Emitter::prepareSubEmitters(const std::vector<EmitterConfig::SubEmitter>& previous) {
    std::vector<std::unique_ptr<Emitter>> kept(config.subEmitters.size());
    for (std::size_t index = 0; index < config.subEmitters.size(); ++index) {
        const EmitterConfig::SubEmitter& sub = config.subEmitters[index];
        if (index < previous.size() && previous[index].config == sub.config && previous[index].inheritColor == sub.inheritColor && index < children.size()) {
            kept[index] = std::move(children[index]);
            continue;
        }
        EmitterConfig child = *sub.config;
        child.localSpace = false;
        child.prewarm = 0.0F;
        child.delay = 0.0F;
        child.light.reset();
        auto made = std::make_unique<Emitter>(std::move(child), seed * 31U + index + 1U);
        made->emitting = false;
        made->inheritsColor = sub.inheritColor;
        made->prepare();
        kept[index] = std::move(made);
    }
    children = std::move(kept);
}

void Emitter::restart() {
    clear();
    time = 0.0F;
    delayLeft = config.delay;
    clock = 0.0F;
    trailClock = 0.0F;
    nextBurst = 0;
    spawned = 0;
    peakCount = 0;
    prewarmed = false;
    emitting = true;
    if (children.size() != config.subEmitters.size()) {
        prepareSubEmitters({});
    }
}

void Emitter::prewarm() {
    if (prewarmed) {
        return;
    }
    prewarmed = true;
    previousPosition = position;
    for (float elapsed = 0.0F; elapsed < config.prewarm; elapsed += kPrewarmStep) {
        step(std::min(kPrewarmStep, config.prewarm - elapsed), nullptr);
    }
}

bool Emitter::isAlive() const noexcept {
    return emitting || getCount() > 0 || std::any_of(children.begin(), children.end(), [](const std::unique_ptr<Emitter>& child) { return child->isAlive(); });
}

math::Vec2 Emitter::getCenter() const noexcept {
    return config.localSpace ? math::Vec2{} : position;
}

math::Vec2 Emitter::getOrigin() const noexcept {
    return config.localSpace ? position : math::Vec2{};
}

float Emitter::pick(math::FloatRange range) {
    return range.min == range.max ? range.min : random.range(range.min, range.max);
}

std::size_t Emitter::pick(EmitterConfig::CountRange range) {
    if (range.min == range.max) {
        return range.min;
    }
    return range.min + static_cast<std::size_t>(random.nextU64() % (range.max - range.min + 1));
}

void Emitter::setAttractor(std::size_t index, math::Vec2 value) {
    if (index >= config.attractors.size()) {
        throw std::out_of_range("The particle emitter has no attractor at that index.");
    }
    config.attractors[index].position = value;
}

void Emitter::setShapePoints(std::vector<math::Vec2> points) {
    EmitterConfig next = config;
    next.shapePoints = std::move(points);
    validateShape(next);
    config.shapePoints = std::move(next.shapePoints);
    prepareShape();
}

void Emitter::setCollisionWorld(const physics2d::World* world, const physics2d::CollisionFilter& filter) {
    collisionWorld = world;
    collisionFilter = filter;
}

void Emitter::spawn(std::size_t count, math::Vec2 from, math::Vec2 to, math::Vec2 inherited, math::Color tint) {
    const std::size_t room = config.maxParticles - std::min(getCount(), config.maxParticles);
    const std::size_t made = std::min(count, room);
    for (std::size_t index = 0; index < made; ++index) {
        spawnParticle(math::Vec2::lerp(from, to, static_cast<float>(index + 1) / static_cast<float>(made)), inherited, tint);
    }
}

// Picks a point of the spawn area before the emitter turns and scales it.
math::Vec2 Emitter::pickShapePoint(std::size_t& pixel) {
    using Shape = EmitterConfig::Shape;
    const math::Vec2 size = config.shapeSize;
    const float thickness = config.shapeThickness > 0.0F ? random.range(-0.5F, 0.5F) * config.shapeThickness : 0.0F;
    switch (config.shape) {
    case Shape::Point:
    case Shape::Cone:
        return {};
    case Shape::Circle:
        // The square root keeps the density even across the disc.
        return math::Vec2::fromAngle(random.range(0.0F, math::Math::kTau), size.x * std::sqrt(random.nextFloat()));
    case Shape::Ring:
        return math::Vec2::fromAngle(random.range(0.0F, math::Math::kTau), size.x + thickness);
    case Shape::Rectangle:
        return {random.range(-size.x, size.x), random.range(-size.y, size.y)};
    case Shape::Ellipse: {
        const math::Vec2 unit = math::Vec2::fromAngle(random.range(0.0F, math::Math::kTau), std::sqrt(random.nextFloat()));
        return {unit.x * size.x, unit.y * size.y};
    }
    case Shape::RectangleEdge: {
        // A walk along the outline from the top-left corner picks each side by its length.
        float along = random.range(0.0F, 4.0F * (size.x + size.y));
        const std::array<std::pair<math::Vec2, math::Vec2>, 4> sides{{{{-size.x, -size.y}, {1.0F, 0.0F}}, {{size.x, -size.y}, {0.0F, 1.0F}}, {{size.x, size.y}, {-1.0F, 0.0F}}, {{-size.x, size.y}, {0.0F, -1.0F}}}};
        for (std::size_t side = 0; side < sides.size(); ++side) {
            const float length = side % 2 == 0 ? size.x * 2.0F : size.y * 2.0F;
            if (along <= length || side == 3) {
                const auto& [corner, direction] = sides[side];
                return corner + direction * along + math::Vec2{-direction.y, direction.x} * thickness;
            }
            along -= length;
        }
        return {};
    }
    case Shape::Arc:
        return math::Vec2::fromAngle(random.range(config.shapeArc.min, config.shapeArc.max), size.x + thickness);
    case Shape::Polygon: {
        const float target = random.range(0.0F, shapeWeights[shapeWeights.size() - 4]);
        std::size_t triangle = 0;
        while (triangle + 4 < shapeWeights.size() && shapeWeights[triangle] < target) {
            triangle += 4;
        }
        const math::Vec2 a = config.shapePoints[static_cast<std::size_t>(shapeWeights[triangle + 1])];
        const math::Vec2 b = config.shapePoints[static_cast<std::size_t>(shapeWeights[triangle + 2])];
        const math::Vec2 c = config.shapePoints[static_cast<std::size_t>(shapeWeights[triangle + 3])];
        float u = random.nextFloat();
        float v = random.nextFloat();
        if (u + v > 1.0F) {
            u = 1.0F - u;
            v = 1.0F - v;
        }
        return a + (b - a) * u + (c - a) * v;
    }
    case Shape::Polyline: {
        const float target = random.range(0.0F, shapeWeights.back());
        const auto found = std::lower_bound(shapeWeights.begin(), shapeWeights.end(), target);
        const auto segment = static_cast<std::size_t>(std::min<std::ptrdiff_t>(found - shapeWeights.begin(), static_cast<std::ptrdiff_t>(shapeWeights.size()) - 1));
        const math::Vec2 from = config.shapePoints[segment];
        const math::Vec2 to = config.shapePoints[segment + 1];
        const math::Vec2 across = (to - from).getNormalized().getPerpendicular();
        return math::Vec2::lerp(from, to, random.nextFloat()) + across * thickness;
    }
    case Shape::Image: {
        const ImageShape& image = *config.shapeImage;
        pixel = static_cast<std::size_t>(random.nextU64() % image.getPoints().size());
        const math::Vec2 scaleOfPixel = size.isZero() ? math::Vec2{1.0F, 1.0F} : size / image.getSize();
        const math::Vec2 jitter{random.range(-0.5F, 0.5F), random.range(-0.5F, 0.5F)};
        const math::Vec2 point = image.getPoints()[pixel] + jitter;
        return {point.x * scaleOfPixel.x, point.y * scaleOfPixel.y};
    }
    }
    return {};
}

Emitter::Spawn Emitter::pickSpawn() {
    using Mode = EmitterConfig::DirectionMode;
    Spawn result;
    const math::Vec2 point = pickShapePoint(result.pixel);
    result.offset = point.rotated(config.shapeAngle + rotation) * scale;
    const float jitter = random.range(-config.spread, config.spread) * 0.5F;
    const float outward = result.offset.isZero() ? random.range(0.0F, math::Math::kTau) : result.offset.getAngle();
    switch (config.directionMode) {
    case Mode::Fixed:
        result.angle = config.direction + rotation + jitter;
        break;
    case Mode::Outward:
        result.angle = outward + jitter;
        break;
    case Mode::Inward:
        result.angle = outward + math::Math::kPi + jitter;
        break;
    case Mode::Tangent:
        result.angle = outward + math::Math::kPi * 0.5F + jitter;
        break;
    }
    if (config.shape == EmitterConfig::Shape::Cone) {
        result.offset = math::Vec2::fromAngle(result.angle, config.shapeSize.x * scale * std::sqrt(random.nextFloat()));
    }
    return result;
}

math::Color Emitter::pickTint(std::size_t pixel) {
    if (config.colorFromImage && config.shape == EmitterConfig::Shape::Image) {
        return config.shapeImage->getColors()[pixel];
    }
    const std::vector<math::Color>& tints = config.tints;
    if (tints.empty()) {
        return math::Color::white();
    }
    switch (config.tintMode) {
    case EmitterConfig::TintMode::Random:
        return tints[static_cast<std::size_t>(random.nextU64() % tints.size())];
    case EmitterConfig::TintMode::Cycle:
        return tints[spawned % tints.size()];
    case EmitterConfig::TintMode::CycleTime:
        break;
    }
    // Without a duration the tints take turns every second.
    const float progress = config.duration > 0.0F ? time / config.duration : time - std::floor(time);
    return tints[std::min(static_cast<std::size_t>(progress * static_cast<float>(tints.size())), tints.size() - 1)];
}

void Emitter::spawnParticle(math::Vec2 origin, math::Vec2 inherited, math::Color tint) {
    const Spawn placed = pickSpawn();
    const float speed = pick(config.speed) * scale;
    const float startSize = pick(config.startSize) * scale;
    particles.positions.push_back(config.localSpace ? placed.offset : origin + placed.offset);
    particles.velocities.push_back(math::Vec2::fromAngle(placed.angle, speed) + inherited);
    particles.ages.push_back(0.0F);
    particles.lifetimes.push_back(pick(config.lifetime));
    particles.startSizes.push_back(startSize);
    particles.endSizes.push_back(config.endSizeScale ? startSize * pick(*config.endSizeScale) : pick(config.endSize) * scale);
    particles.rotations.push_back(pick(config.rotation) + rotation);
    particles.spins.push_back(pick(config.spin));
    particles.radial.push_back(pick(config.radialAcceleration) * scale);
    particles.tangential.push_back(pick(config.tangentialAcceleration) * scale);
    if (usesTints) {
        particles.tints.push_back(pickTint(placed.pixel) * tint);
    }
    if (usesAspects) {
        particles.aspects.push_back(pick(config.aspect));
    }
    if (usesFrameStarts) {
        particles.frames.push_back(frames.empty() ? 0U : static_cast<std::uint32_t>(random.nextU64() % frames.size()));
    }
    if (usesHits) {
        particles.flags.push_back(0);
    }
    if (config.trail.length > 0) {
        particles.trailCounts.push_back(0);
        particles.trails.resize(particles.trails.size() + config.trail.length);
    }
    ++spawned;

    for (std::size_t index = 0; index < config.subEmitters.size(); ++index) {
        if (config.subEmitters[index].trigger == EmitterConfig::SubEmitter::Trigger::Birth) {
            trigger(index, getCount() - 1);
        }
    }
}

void Emitter::burst(std::size_t count) {
    spawn(count, position, position, velocity * config.inheritVelocity, math::Color::white());
    liveParticles.set(getCount());
}

// Spawns the particles of a sub-emitter where a particle of this emitter is, with the velocity and the color it passes on.
void Emitter::trigger(std::size_t subEmitter, std::size_t index) {
    const EmitterConfig::SubEmitter& sub = config.subEmitters[subEmitter];
    if (sub.probability < 1.0F && !random.chance(sub.probability)) {
        return;
    }
    const std::size_t count = pick(sub.count);
    const math::Vec2 at = getOrigin() + particles.positions[index];
    const math::Vec2 inherited = getVelocity(index) * sub.inheritVelocity;
    const math::Color tint = sub.inheritColor ? particleColor(index) : math::Color::white();
    Emitter& child = *children[subEmitter];
    child.spawn(count, at, at, inherited, tint);
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
    velocity = deltaSeconds > 0.0F ? (position - previousPosition) / deltaSeconds : math::Vec2{};
    clock += deltaSeconds;
    const bool castsRays = config.collision.type == EmitterConfig::Collision::Type::World && collisionWorld != nullptr;
    if (castsRays) {
        previousPositions.assign(particles.positions.begin(), particles.positions.end());
    }

    if (jobs == nullptr) {
        simulate(deltaSeconds, 0, getCount());
    } else {
        // clang-format off
        jobs->parallelFor(0, getCount(), kParallelGrain, [this, deltaSeconds](std::size_t begin, std::size_t end) {
            simulate(deltaSeconds, begin, end);
        });
        // clang-format on
    }
    if (castsRays) {
        collideWithWorld(jobs);
    }
    runTriggers(deltaSeconds);
    removeExpired();
    sampleTrails(deltaSeconds);
    emit(deltaSeconds);
    previousPosition = position;
    peakCount = std::max(peakCount, getCount());
    for (const std::unique_ptr<Emitter>& child : children) {
        child->step(deltaSeconds, jobs);
    }
    liveParticles.set(getCount());
}

// Moves the particles of one chunk. Everything here only touches the particles of the chunk, so chunks run on any thread.
void Emitter::simulate(float deltaSeconds, std::size_t begin, std::size_t end) noexcept {
    const float damping = std::exp(-config.damping * deltaSeconds);
    const math::Vec2 center = getCenter();
    const math::Vec2 gravity = config.gravity * scale;
    const EmitterConfig::Turbulence& turbulence = config.turbulence;
    const float flow = clock * turbulence.speed;
    const bool sticks = usesHits;
    const EmitterConfig::Collision& collision = config.collision;
    for (std::size_t index = begin; index < end; ++index) {
        particles.ages[index] += deltaSeconds;
        if (sticks) {
            particles.flags[index] &= static_cast<std::uint8_t>(~kHit);
            if ((particles.flags[index] & kStuck) != 0) {
                continue;
            }
        }

        math::Vec2& point = particles.positions[index];
        math::Vec2& motion = particles.velocities[index];
        math::Vec2 acceleration = gravity;
        if (particles.radial[index] != 0.0F || particles.tangential[index] != 0.0F) {
            const math::Vec2 outward = (point - center).getNormalized();
            acceleration += outward * particles.radial[index] + math::Vec2{-outward.y, outward.x} * particles.tangential[index];
        }
        if (turbulence.strength != 0.0F) {
            const math::Vec2 sample = (config.localSpace ? point + position : point) * turbulence.frequency;
            acceleration += math::Vec2{noise.simplex(sample.x + flow, sample.y), noise.simplex(sample.x - 37.1F, sample.y + 11.3F + flow)} * (turbulence.strength * scale);
        }
        for (const EmitterConfig::Attractor& attractor : config.attractors) {
            const math::Vec2 world = attractor.space == EmitterConfig::Attractor::Space::World ? attractor.position : position + attractor.position.rotated(rotation) * scale;
            const math::Vec2 toward = world - (config.localSpace ? position : math::Vec2{}) - point;
            const float distance = toward.getLength();
            if (distance <= attractor.killRadius * scale) {
                particles.ages[index] = particles.lifetimes[index];
                break;
            }
            const float reach = attractor.radius * scale;
            if (distance < reach) {
                acceleration += toward / distance * (attractor.strength * scale * (1.0F - distance / reach));
            }
        }

        const float life = std::min(particles.ages[index] / particles.lifetimes[index], 1.0F);
        motion = (motion + acceleration * deltaSeconds) * damping;
        point += motion * (easeDown(config.speedCurve, life) * deltaSeconds);
        particles.rotations[index] += particles.spins[index] * (easeDown(config.spinCurve, life) * deltaSeconds);

        if (collision.type == EmitterConfig::Collision::Type::Floor) {
            const float floor = center.y + collision.y * scale;
            if (point.y > floor) {
                collide(index, {point.x, floor}, {0.0F, -1.0F});
            }
        } else if (collision.type == EmitterConfig::Collision::Type::Bounds) {
            const math::Rect area{center.x + collision.area.x * scale, center.y + collision.area.y * scale, collision.area.width * scale, collision.area.height * scale};
            if (point.x < area.getLeft()) {
                collide(index, {area.getLeft(), point.y}, {1.0F, 0.0F});
            } else if (point.x > area.getRight()) {
                collide(index, {area.getRight(), point.y}, {-1.0F, 0.0F});
            }
            if (point.y < area.getTop()) {
                collide(index, {point.x, area.getTop()}, {0.0F, 1.0F});
            } else if (point.y > area.getBottom()) {
                collide(index, {point.x, area.getBottom()}, {0.0F, -1.0F});
            }
        }
        if (config.bounds) {
            const math::Rect area{center.x + config.bounds->x * scale, center.y + config.bounds->y * scale, config.bounds->width * scale, config.bounds->height * scale};
            if (!area.contains(point)) {
                if (config.boundsMode == EmitterConfig::BoundsMode::Kill) {
                    particles.ages[index] = particles.lifetimes[index];
                } else {
                    point.x = area.x + std::fmod(std::fmod(point.x - area.x, area.width) + area.width, area.width);
                    point.y = area.y + std::fmod(std::fmod(point.y - area.y, area.height) + area.height, area.height);
                }
            }
        }
    }
}

// Places a particle against the surface it hit and bounces, sticks or kills it. Slow hits only stop it against the surface.
void Emitter::collide(std::size_t index, math::Vec2 contact, math::Vec2 normal) noexcept {
    using Result = EmitterConfig::Collision::Result;
    const EmitterConfig::Collision& collision = config.collision;
    math::Vec2& motion = particles.velocities[index];
    particles.positions[index] = contact;
    const float into = math::Vec2::dot(motion, normal);
    if (into >= -kRestingSpeed * scale) {
        motion -= normal * std::min(into, 0.0F);
        return;
    }

    particles.flags[index] |= kHit;
    particles.ages[index] += collision.lifeLoss * particles.lifetimes[index];
    switch (collision.result) {
    case Result::Bounce: {
        const math::Vec2 along = motion - normal * into;
        motion = along * (1.0F - collision.friction) - normal * (into * collision.bounce);
        break;
    }
    case Result::Stick:
        motion = {};
        particles.flags[index] |= kStuck;
        break;
    case Result::Die:
        particles.ages[index] = particles.lifetimes[index];
        break;
    }
}

// Casts the move of every particle of the step through the physics world, on the job system when it is given.
void Emitter::collideWithWorld(core::JobSystem* jobs) {
    const math::Vec2 origin = getOrigin();
    const std::size_t count = getCount();
    rays.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        const bool moving = (particles.flags[index] & kStuck) == 0;
        rays.setRay(index, origin + previousPositions[index], origin + (moving ? particles.positions[index] : previousPositions[index]));
    }
    physics2d::Raycaster(*collisionWorld).castBatch(rays, collisionFilter, jobs);
    for (std::size_t index = 0; index < count; ++index) {
        if (const std::optional<physics2d::RaycastHit>& hit = rays.getHit(index)) {
            collide(index, hit->point - origin + hit->normal * 0.01F, hit->normal);
        }
    }
}

// Fires the sub-emitters of the hits of this step and the ones that spawn while their particles live.
void Emitter::runTriggers(float deltaSeconds) {
    using Trigger = EmitterConfig::SubEmitter::Trigger;
    for (std::size_t sub = 0; sub < config.subEmitters.size(); ++sub) {
        const EmitterConfig::SubEmitter& entry = config.subEmitters[sub];
        if (entry.trigger == Trigger::Collision && usesHits) {
            for (std::size_t index = 0; index < getCount(); ++index) {
                if ((particles.flags[index] & kHit) != 0) {
                    trigger(sub, index);
                }
            }
        } else if (entry.trigger == Trigger::Alive) {
            for (std::size_t index = 0; index < getCount(); ++index) {
                const auto times = static_cast<std::size_t>(entry.rate * deltaSeconds + random.nextFloat());
                for (std::size_t repeat = 0; repeat < times; ++repeat) {
                    trigger(sub, index);
                }
            }
        }
    }
}

// Fires the death sub-emitters, then compacts the arrays in place, which keeps the order particles draw in.
void Emitter::removeExpired() {
    for (std::size_t sub = 0; sub < config.subEmitters.size(); ++sub) {
        if (config.subEmitters[sub].trigger == EmitterConfig::SubEmitter::Trigger::Death) {
            for (std::size_t index = 0; index < getCount(); ++index) {
                if (particles.ages[index] >= particles.lifetimes[index]) {
                    trigger(sub, index);
                }
            }
        }
    }

    const std::size_t length = config.trail.length;
    std::size_t kept = 0;
    for (std::size_t index = 0; index < particles.ages.size(); ++index) {
        if (particles.ages[index] >= particles.lifetimes[index]) {
            continue;
        }
        if (kept != index) {
            // clang-format off
            particles.forEachArray([kept, index](auto& values) {
                if (!values.empty()) {
                    values[kept] = values[index];
                }
            });
            // clang-format on
            std::copy_n(particles.trails.begin() + static_cast<std::ptrdiff_t>(index * length), length, particles.trails.begin() + static_cast<std::ptrdiff_t>(kept * length));
        }
        ++kept;
    }
    // clang-format off
    particles.forEachArray([kept](auto& values) {
        if (!values.empty()) {
            values.resize(kept);
        }
    });
    // clang-format on
    particles.trails.resize(kept * length);
}

// Every particle records its position at the same moments, so each point of a trail is as old as the same point of every other trail.
void Emitter::sampleTrails(float deltaSeconds) {
    const std::size_t length = config.trail.length;
    if (length == 0) {
        return;
    }
    const float interval = config.trail.lifetime / static_cast<float>(length);
    trailClock += deltaSeconds;
    if (trailClock < interval) {
        return;
    }
    trailClock = std::fmod(trailClock, interval);
    for (std::size_t index = 0; index < getCount(); ++index) {
        math::Vec2* block = particles.trails.data() + index * length;
        std::memmove(block + 1, block, (length - 1) * sizeof(math::Vec2));
        block[0] = particles.positions[index];
        particles.trailCounts[index] = static_cast<std::uint16_t>(std::min<std::size_t>(particles.trailCounts[index] + 1U, length));
    }
}

double Emitter::takeBursts() {
    double count = 0.0;
    while (nextBurst < firings.size() && firings[nextBurst].time <= time) {
        const EmitterConfig::Burst& planned = config.bursts[firings[nextBurst++].burst];
        if (planned.probability >= 1.0F || random.chance(planned.probability)) {
            count += static_cast<double>(pick(planned.count));
        }
    }
    return count;
}

// Follows the emission cycle to the end of the frame, so bursts and the rate keep their pace even when one frame spans several loops. The whole loops inside a long frame count at once with the average count of their bursts. Particles of the rate and of the distance spread along the segment the emitter moved, while bursts spawn at its position.
void Emitter::emit(float deltaSeconds) {
    if (!emitting) {
        emitDebt = 0.0F;
        distanceDebt = 0.0F;
        return;
    }
    if (!(deltaSeconds > 0.0F)) {
        return;
    }
    if (delayLeft > 0.0F) {
        delayLeft -= deltaSeconds;
        if (delayLeft > 0.0F) {
            return;
        }
        deltaSeconds = -delayLeft;
        delayLeft = 0.0F;
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
            for (const Firing& firing : firings) {
                const EmitterConfig::Burst& planned = config.bursts[firing.burst];
                everyBurst += static_cast<double>(planned.probability) * 0.5 * static_cast<double>(planned.count.min + planned.count.max);
            }
            due += everyBurst * static_cast<double>(std::round((rest - partial) / duration));
            if (partial > 0.0F) {
                time = partial;
                nextBurst = 0;
                due += takeBursts();
            }
        }
    }

    const math::Vec2 inherited = velocity * config.inheritVelocity;
    const auto limit = static_cast<double>(config.maxParticles);
    spawn(due < limit ? static_cast<std::size_t>(due) : config.maxParticles, position, position, inherited, math::Color::white());

    // Fractional particles carry over, so low rates still emit on average at the requested pace.
    emitDebt += config.rate * emitted;
    distanceDebt += math::Vec2::distance(previousPosition, position) * config.rateOverDistance;
    const float whole = std::floor(emitDebt) + std::floor(distanceDebt);
    emitDebt -= std::floor(emitDebt);
    distanceDebt -= std::floor(distanceDebt);
    spawn(static_cast<double>(whole) < limit ? static_cast<std::size_t>(whole) : config.maxParticles, previousPosition, position, inherited, math::Color::white());
}

math::Color Emitter::colorAt(float life) const noexcept {
    const std::vector<math::Color>& colors = config.colors;
    const std::size_t count = colors.size();
    if (count == 1) {
        return colors.front();
    }
    const float progress = math::Math::saturate(life);
    const bool steps = config.colorBlend == EmitterConfig::ColorBlend::Steps;
    if (config.colorTimes.empty()) {
        if (steps) {
            return colors[std::min(static_cast<std::size_t>(progress * static_cast<float>(count)), count - 1)];
        }
        const float scaled = progress * static_cast<float>(count - 1);
        const auto index = std::min(static_cast<std::size_t>(scaled), count - 2);
        return math::Color::lerp(colors[index], colors[index + 1], scaled - static_cast<float>(index));
    }

    const std::vector<float>& times = config.colorTimes;
    const auto after = std::upper_bound(times.begin(), times.end(), progress);
    if (after == times.begin()) {
        return colors.front();
    }
    const auto index = static_cast<std::size_t>(after - times.begin()) - 1;
    if (index + 1 >= count || steps) {
        return colors[index];
    }
    const float span = times[index + 1] - times[index];
    return span > 0.0F ? math::Color::lerp(colors[index], colors[index + 1], (progress - times[index]) / span) : colors[index + 1];
}

math::Color Emitter::particleColor(std::size_t index) const noexcept {
    const math::Color color = colorAt(particles.ages[index] / particles.lifetimes[index]);
    return particles.tints.empty() ? color : color * particles.tints[index];
}

math::Rect Emitter::frameAt(std::size_t index, float life) const noexcept {
    using Mode = EmitterConfig::FrameMode;
    if (frames.empty()) {
        return {};
    }
    const std::size_t count = frames.size();
    const auto played = static_cast<std::size_t>(particles.ages[index] * config.frameRate);
    switch (config.frameMode) {
    case Mode::OverLife:
        return frames[std::min(static_cast<std::size_t>(life * static_cast<float>(count)), count - 1)];
    case Mode::Loop:
        return frames[played % count];
    case Mode::LoopRandomStart:
        return frames[(particles.frames[index] + played) % count];
    case Mode::Random:
        return frames[particles.frames[index] % count];
    }
    return frames.front();
}

// The sprite of a particle: its size along the curve, stretched along its motion, turned with its motion and snapped to the steps and the pixel grid.
graphics2d::SpriteInstance Emitter::spriteAt(std::size_t index) const noexcept {
    const float life = std::min(particles.ages[index] / particles.lifetimes[index], 1.0F);
    const float eased = config.sizeCurve.isLinear() ? life : config.sizeCurve.apply(life);
    const float size = particles.startSizes[index] + (particles.endSizes[index] - particles.startSizes[index]) * eased;
    const math::Vec2 heading = particles.velocities[index];

    // Particles keep facing their heading while the speed curve slows them, and stretch with the speed they have.
    float angle = particles.rotations[index];
    if (config.alignToVelocity && !heading.isZero()) {
        angle += heading.getAngle();
    }
    if (config.rotationStep > 0.0F) {
        angle = std::round(angle / config.rotationStep) * config.rotationStep;
    }
    float width = particles.aspects.empty() ? size : size * particles.aspects[index];
    if (config.stretch > 0.0F) {
        width += heading.getLength() * std::fabs(easeDown(config.speedCurve, life)) * config.stretch;
    }
    math::Vec2 at = getOrigin() + particles.positions[index];
    if (config.pixelSnap > 0.0F) {
        at = math::Vec2{std::round(at.x / config.pixelSnap), std::round(at.y / config.pixelSnap)} * config.pixelSnap;
    }
    return {.position = at, .size = {width, size}, .source = frameAt(index, life), .rotation = angle, .color = particleColor(index)};
}

Emitter::Particle Emitter::getParticle(std::size_t index) const {
    if (index >= getCount()) {
        throw std::out_of_range("The particle emitter has no live particle at that index.");
    }
    return {.position = getOrigin() + particles.positions[index], .velocity = getVelocity(index), .age = particles.ages[index], .lifetime = particles.lifetimes[index], .sprite = spriteAt(index)};
}

// The velocity a particle moves with, which the speed curve scales.
math::Vec2 Emitter::getVelocity(std::size_t index) const noexcept {
    return particles.velocities[index] * easeDown(config.speedCurve, std::min(particles.ages[index] / particles.lifetimes[index], 1.0F));
}

float Emitter::easeDown(const std::optional<math::EasingCurve>& curve, float life) noexcept {
    return curve ? 1.0F - curve->apply(life) : 1.0F;
}

void Emitter::draw(graphics2d::Renderer& renderer) const {
    if (getCount() > 0) {
        instances.clear();
        const bool newestFirst = config.particleOrder == EmitterConfig::ParticleOrder::NewestFirst;
        for (std::size_t drawn = 0; drawn < getCount(); ++drawn) {
            instances.push_back(spriteAt(newestFirst ? getCount() - 1 - drawn : drawn));
        }
        renderer.drawBatch(config.texture, instances, config.order);
        drawTrails(renderer);
        drawLights(renderer);
    }
    for (const std::unique_ptr<Emitter>& child : children) {
        child->draw(renderer);
    }
}

// The trails of every particle draw as one mesh, from each particle through the points it left behind.
void Emitter::drawTrails(graphics2d::Renderer& renderer) const {
    const std::size_t length = config.trail.length;
    if (length == 0) {
        return;
    }
    trailVertices.clear();
    trailIndices.clear();
    trailPlaces.resize(length + 1);
    for (std::size_t point = 0; point <= length; ++point) {
        trailPlaces[point] = static_cast<float>(point) / static_cast<float>(length);
    }
    const math::Vec2 origin = getOrigin();
    const EmitterConfig::ParticleTrail& trail = config.trail;
    for (std::size_t index = 0; index < getCount(); ++index) {
        const std::size_t points = particles.trailCounts[index];
        if (points == 0) {
            continue;
        }
        trailLine.clear();
        trailLine.push_back(origin + particles.positions[index]);
        for (std::size_t point = 0; point < points; ++point) {
            trailLine.push_back(origin + particles.trails[index * length + point]);
        }
        Ribbon::append(trailLine, std::span<const float>(trailPlaces).first(trailLine.size()), {.widthStart = trail.widthStart * scale, .widthEnd = trail.widthEnd * scale, .colors = trail.colors, .tint = particleColor(index)}, trailVertices, trailIndices);
    }
    if (!trailIndices.empty()) {
        renderer.drawMesh(trail.texture, trailVertices, trailIndices, config.order);
    }
}

// Lights only exist in lit canvases, so other canvases draw the particles alone.
void Emitter::drawLights(graphics2d::Renderer& renderer) const {
    if ((!config.light && !config.particleLights) || !renderer.isCanvasLit()) {
        return;
    }
    if (config.light) {
        const EmitterConfig::Light& glow = *config.light;
        float strength = glow.intensity;
        if (glow.flicker.amount > 0.0F) {
            strength *= lighting2d::LightFlicker::intensity(clock, glow.flicker.speed, glow.flicker.amount, seed);
        }
        if (glow.fade == EmitterConfig::Light::Fade::Cycle && !emitting) {
            strength = 0.0F;
        } else if (glow.fade == EmitterConfig::Light::Fade::Count) {
            strength *= peakCount > 0 ? static_cast<float>(getCount()) / static_cast<float>(peakCount) : 0.0F;
        }
        if (strength > 0.0F) {
            renderer.drawLight({.type = glow.type, .position = position + glow.offset.rotated(rotation) * scale, .radius = glow.radius * scale, .color = glow.color, .intensity = strength});
        }
    }
    if (config.particleLights && config.particleLights->max > 0) {
        const EmitterConfig::ParticleLights& lights = *config.particleLights;
        const math::Vec2 origin = getOrigin();
        const std::size_t shown = std::min(lights.max, getCount());
        for (std::size_t light = 0; light < shown; ++light) {
            const std::size_t index = light * getCount() / shown;
            const math::Color color = particleColor(index);
            if (color.a > 0.0F) {
                renderer.drawLight({.position = origin + particles.positions[index], .radius = lights.radius * scale, .color = color.withAlpha(1.0F), .intensity = lights.intensity * color.a});
            }
        }
    }
}

void Emitter::clear() noexcept {
    particles.forEachArray([](auto& values) { values.clear(); });
    particles.trails.clear();
    emitDebt = 0.0F;
    distanceDebt = 0.0F;
    for (const std::unique_ptr<Emitter>& child : children) {
        child->clear();
    }
    liveParticles.set(0);
}

} // namespace haylen::particles2d
