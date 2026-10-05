#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::particles2d {

class ImageShape;

// Describes how particles are born, move, look and die. Directions, spreads and rotations are in radians, sizes and distances in world units, and every value that takes a range picks one value per particle.
// An emitter with a duration emits for that many seconds after its delay, firing its bursts at their times, and then stops or starts the cycle again when it loops. Without a duration it emits until the app stops it, and each burst fires once.
struct EmitterConfig {
    enum class Shape : std::uint8_t {
        Point,
        Circle,
        Ring,
        Rectangle,
        Cone,
        Ellipse,
        RectangleEdge,
        Arc,
        Polygon,
        Polyline,
        Image,
    };

    // Particles leave along the direction and spread, or away from the center of the spawn area, toward it, or along its outline, with the spread around that angle.
    enum class DirectionMode : std::uint8_t {
        Fixed,
        Outward,
        Inward,
        Tangent,
    };

    // Colors blend into the next color, or hold until the next color starts.
    enum class ColorBlend : std::uint8_t {
        Smooth,
        Steps,
    };

    // A new particle picks its tint at random, in order, or by its moment in the emission cycle.
    enum class TintMode : std::uint8_t {
        Random,
        Cycle,
        CycleTime,
    };

    // Frames play once across the life, loop at the frame rate, loop from a random frame, or stay on one random frame.
    enum class FrameMode : std::uint8_t {
        OverLife,
        Loop,
        LoopRandomStart,
        Random,
    };

    // New particles draw on top of the older ones, or below them.
    enum class ParticleOrder : std::uint8_t {
        OldestFirst,
        NewestFirst,
    };

    // Particles that leave the bounds die, or come back on the other side.
    enum class BoundsMode : std::uint8_t {
        Kill,
        Wrap,
    };

    struct CountRange {
        std::size_t min = 0;
        std::size_t max = 0;
    };

    // A burst repeats `cycles` times `interval` seconds apart, and each time it fires with the probability, picking a count in its range.
    struct Burst {
        float time = 0.0F;
        CountRange count{};
        int cycles = 1;
        float interval = 0.0F;
        float probability = 1.0F;
    };

    // Cuts the texture into columns and rows of equal frames, read row by row, of which the first `count` play, or all of them when the count is 0.
    struct FrameGrid {
        int columns = 0;
        int rows = 0;
        int count = 0;
    };

    // Noise that pushes particles around, sampled at their positions, scaled by the frequency, and moving with the time at the speed.
    struct Turbulence {
        float strength = 0.0F;
        float frequency = 0.01F;
        float speed = 0.5F;
    };

    // Pulls the particles inside its radius toward its position, harder the closer they get, and removes those that reach the kill radius.
    struct Attractor {
        // The position is relative to the emitter, turning and scaling with it, or a point of the world.
        enum class Space : std::uint8_t {
            Emitter,
            World,
        };

        math::Vec2 position{};
        float strength = 0.0F;
        float radius = 100.0F;
        float killRadius = 0.0F;
        Space space = Space::Emitter;
    };

    // Particles hit a floor at `y` below the emitter, the walls of the area around the emitter, or the shapes of the physics world the app gives the emitter, and then bounce, stick or die. Each hit takes `lifeLoss` of the lifetime.
    struct Collision {
        enum class Type : std::uint8_t {
            None,
            Floor,
            Bounds,
            World,
        };

        enum class Result : std::uint8_t {
            Bounce,
            Stick,
            Die,
        };

        Type type = Type::None;
        float y = 0.0F;
        math::Rect area{};
        float bounce = 0.5F;
        float friction = 0.0F;
        Result result = Result::Bounce;
        float lifeLoss = 0.0F;
    };

    // Spawns the particles of another emitter where particles of this one are born, die, hit something or, at the rate per second, while they live.
    struct SubEmitter {
        enum class Trigger : std::uint8_t {
            Birth,
            Death,
            Collision,
            Alive,
        };

        std::shared_ptr<EmitterConfig> config;
        Trigger trigger = Trigger::Death;
        CountRange count{1, 1};
        float rate = 10.0F;
        float probability = 1.0F;
        float inheritVelocity = 0.0F;
        bool inheritColor = false;
    };

    // A light of lit canvases at the emitter, which can waver like a flame and follow the emission cycle or the share of live particles.
    struct Light {
        enum class Fade : std::uint8_t {
            None,
            Cycle,
            Count,
        };

        struct Flicker {
            float speed = 0.0F;
            float amount = 0.0F;
        };

        lighting2d::Light::Type type = lighting2d::Light::Type::Point;
        math::Vec2 offset{};
        float radius = 160.0F;
        math::Color color = math::Color::white();
        float intensity = 1.0F;
        Flicker flicker{};
        Fade fade = Fade::None;
    };

    // A small light of lit canvases on up to `max` particles, in their color and fading with their alpha.
    struct ParticleLights {
        float radius = 24.0F;
        float intensity = 0.5F;
        std::size_t max = 16;
    };

    // A ribbon through the last `length` positions of each particle, taken over `lifetime` seconds, whose width and colors go from the particle to the end of the ribbon.
    struct ParticleTrail {
        std::size_t length = 0;
        float lifetime = 0.25F;
        float widthStart = 6.0F;
        float widthEnd = 0.0F;
        std::vector<math::Color> colors{math::Color::white(), math::Color::transparent()};
        graphics::Texture texture;
    };

    graphics::Texture texture;
    std::vector<math::Rect> frames;
    FrameGrid frameGrid{};
    FrameMode frameMode = FrameMode::OverLife;
    float frameRate = 10.0F;

    float rate = 20.0F;
    float rateOverDistance = 0.0F;
    std::vector<Burst> bursts;
    float delay = 0.0F;
    float duration = 0.0F;
    bool loop = false;
    float prewarm = 0.0F;
    std::size_t maxParticles = 256;

    math::FloatRange lifetime{1.0F, 1.0F};
    math::FloatRange speed{50.0F, 100.0F};

    // Eases the speed from its full value at birth to 0 at the end of the life, as `sizeCurve` eases the size, and `spinCurve` does the same for the spin. Without a curve both stay as they are.
    std::optional<math::EasingCurve> speedCurve;
    float direction = -1.5707964F;
    DirectionMode directionMode = DirectionMode::Fixed;
    float spread = 0.5F;
    float inheritVelocity = 0.0F;
    math::Vec2 gravity{};
    math::FloatRange radialAcceleration{};
    math::FloatRange tangentialAcceleration{};
    float damping = 0.0F;
    Turbulence turbulence{};
    std::vector<Attractor> attractors;
    Collision collision{};
    std::optional<math::Rect> bounds;
    BoundsMode boundsMode = BoundsMode::Kill;

    math::FloatRange startSize{16.0F, 16.0F};
    math::FloatRange endSize{16.0F, 16.0F};
    std::optional<math::FloatRange> endSizeScale;
    math::EasingCurve sizeCurve{math::Easing::Type::Linear};
    math::FloatRange aspect{1.0F, 1.0F};
    float stretch = 0.0F;
    math::FloatRange rotation{};
    float rotationStep = 0.0F;
    bool alignToVelocity = false;
    math::FloatRange spin{};
    std::optional<math::EasingCurve> spinCurve;

    std::vector<math::Color> colors{math::Color::white()};
    std::vector<float> colorTimes;
    ColorBlend colorBlend = ColorBlend::Smooth;
    std::vector<math::Color> tints;
    TintMode tintMode = TintMode::Random;

    Shape shape = Shape::Point;
    math::Vec2 shapeSize{};
    float shapeAngle = 0.0F;
    math::FloatRange shapeArc{0.0F, 6.2831855F};
    std::vector<math::Vec2> shapePoints;
    float shapeThickness = 0.0F;
    std::shared_ptr<ImageShape> shapeImage;
    bool colorFromImage = false;

    bool localSpace = false;
    float pixelSnap = 0.0F;
    ParticleOrder particleOrder = ParticleOrder::OldestFirst;
    std::vector<SubEmitter> subEmitters;
    ParticleTrail trail{};
    std::optional<Light> light;
    std::optional<ParticleLights> particleLights;
    graphics2d::DrawOrder order{};

    // Every enum has one table of its names, which effect files and Lua read and write through `fromName` and `nameOf`.
    static constexpr std::array<std::pair<std::string_view, Shape>, 11> kShapeNames{{{"point", Shape::Point}, {"circle", Shape::Circle}, {"ring", Shape::Ring}, {"rectangle", Shape::Rectangle}, {"cone", Shape::Cone}, {"ellipse", Shape::Ellipse}, {"rectangleEdge", Shape::RectangleEdge}, {"arc", Shape::Arc}, {"polygon", Shape::Polygon}, {"polyline", Shape::Polyline}, {"image", Shape::Image}}};
    static constexpr std::array<std::pair<std::string_view, DirectionMode>, 4> kDirectionModeNames{{{"fixed", DirectionMode::Fixed}, {"outward", DirectionMode::Outward}, {"inward", DirectionMode::Inward}, {"tangent", DirectionMode::Tangent}}};
    static constexpr std::array<std::pair<std::string_view, ColorBlend>, 2> kColorBlendNames{{{"smooth", ColorBlend::Smooth}, {"steps", ColorBlend::Steps}}};
    static constexpr std::array<std::pair<std::string_view, TintMode>, 3> kTintModeNames{{{"random", TintMode::Random}, {"cycle", TintMode::Cycle}, {"cycleTime", TintMode::CycleTime}}};
    static constexpr std::array<std::pair<std::string_view, FrameMode>, 4> kFrameModeNames{{{"overLife", FrameMode::OverLife}, {"loop", FrameMode::Loop}, {"loopRandomStart", FrameMode::LoopRandomStart}, {"random", FrameMode::Random}}};
    static constexpr std::array<std::pair<std::string_view, ParticleOrder>, 2> kParticleOrderNames{{{"oldestFirst", ParticleOrder::OldestFirst}, {"newestFirst", ParticleOrder::NewestFirst}}};
    static constexpr std::array<std::pair<std::string_view, BoundsMode>, 2> kBoundsModeNames{{{"kill", BoundsMode::Kill}, {"wrap", BoundsMode::Wrap}}};
    static constexpr std::array<std::pair<std::string_view, Attractor::Space>, 2> kSpaceNames{{{"emitter", Attractor::Space::Emitter}, {"world", Attractor::Space::World}}};
    static constexpr std::array<std::pair<std::string_view, Collision::Type>, 4> kCollisionTypeNames{{{"none", Collision::Type::None}, {"floor", Collision::Type::Floor}, {"bounds", Collision::Type::Bounds}, {"world", Collision::Type::World}}};
    static constexpr std::array<std::pair<std::string_view, Collision::Result>, 3> kCollisionResultNames{{{"bounce", Collision::Result::Bounce}, {"stick", Collision::Result::Stick}, {"die", Collision::Result::Die}}};
    static constexpr std::array<std::pair<std::string_view, SubEmitter::Trigger>, 4> kTriggerNames{{{"birth", SubEmitter::Trigger::Birth}, {"death", SubEmitter::Trigger::Death}, {"collision", SubEmitter::Trigger::Collision}, {"alive", SubEmitter::Trigger::Alive}}};
    static constexpr std::array<std::pair<std::string_view, Light::Fade>, 3> kFadeNames{{{"none", Light::Fade::None}, {"cycle", Light::Fade::Cycle}, {"count", Light::Fade::Count}}};

    template <typename Enum, std::size_t Size> [[nodiscard]] static constexpr std::optional<Enum> fromName(const std::array<std::pair<std::string_view, Enum>, Size>& names, std::string_view name) noexcept {
        for (const auto& [text, value] : names) {
            if (text == name) {
                return value;
            }
        }
        return std::nullopt;
    }

    template <typename Enum, std::size_t Size> [[nodiscard]] static constexpr std::string_view nameOf(const std::array<std::pair<std::string_view, Enum>, Size>& names, Enum value) noexcept {
        for (const auto& [text, candidate] : names) {
            if (candidate == value) {
                return text;
            }
        }
        return names.front().first;
    }
};

} // namespace haylen::particles2d
