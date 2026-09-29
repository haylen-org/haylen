#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::particles2d {

// Describes how particles are born, move and fade. Directions and spreads are in radians, sizes are in world units, and colors blend evenly across the lifetime. Frames play across the lifetime too.
// Cone emitters place particles inside the circular sector of radius shapeSize.x that the direction and spread describe, and send them straight out of it.
// An emitter with a duration emits for that many seconds, firing its bursts at their times, and then stops or starts the cycle again when it loops. Without a duration it emits until the app stops it, and each burst fires once.
struct EmitterConfig {
    enum class Shape : std::uint8_t {
        Point,
        Circle,
        Ring,
        Rectangle,
        Cone,
    };

    struct Burst {
        float time = 0.0F;
        std::size_t count = 0;
    };

    graphics::Texture texture;
    std::vector<math::Rect> frames;
    float rate = 20.0F;
    std::vector<Burst> bursts;
    float duration = 0.0F;
    bool loop = false;
    float prewarm = 0.0F;
    std::size_t maxParticles = 256;
    math::FloatRange lifetime{1.0F, 1.0F};
    math::FloatRange speed{50.0F, 100.0F};
    float direction = -1.5707964F;
    float spread = 0.5F;
    math::Vec2 gravity{};
    math::FloatRange radialAcceleration{};
    math::FloatRange tangentialAcceleration{};
    float damping = 0.0F;
    math::FloatRange startSize{16.0F, 16.0F};
    math::FloatRange endSize{16.0F, 16.0F};
    math::FloatRange spin{0.0F, 0.0F};
    std::vector<math::Color> colors{math::Color::white()};
    Shape shape = Shape::Point;
    math::Vec2 shapeSize{};
    bool localSpace = false;
    graphics2d::DrawOrder order{};

    // Resolves the shape names "point", "circle", "ring", "rectangle" and "cone".
    [[nodiscard]] static std::optional<Shape> shapeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view shapeName(Shape value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Shape>, 5> kShapeNames;
};

} // namespace haylen::particles2d
