#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// A rectangle with rounded corners or an ellipse, turned around its center, which draws circles, capsules, rings and arcs too. The shader works out how much of every pixel the shape covers from its distance to the edge, so the edge fades over one pixel of the screen at any size, zoom and rotation.
struct Shape {
    // A rectangle rounds its corners by their radii, and an ellipse fills the bounds and leaves the radii unused.
    enum class Kind : std::uint8_t {
        Rectangle,
        Ellipse,
    };

    Kind kind = Kind::Rectangle;

    // The area of the shape before it turns.
    math::Rect bounds{};

    // The radius of the top-left, top-right, bottom-right and bottom-left corners, each at most half the shorter side.
    std::array<float, 4> radii{};
    float rotation = 0.0F;

    // The part that draws: the sector that starts at the start angle, measured clockwise from the x axis of the shape, and turns clockwise by the sweep, where a full turn keeps the whole shape.
    float startAngle = 0.0F;
    float sweep = math::Math::kTau;
    math::Color color = math::Color::white();

    // An image stretched over the bounds under the fill color, such as a portrait in a circle or a gradient, white when empty.
    graphics::Texture texture;

    // The border runs inside the edge, at most half the shorter side wide, over the fill.
    float borderWidth = 0.0F;
    math::Color borderColor = math::Color::transparent();

    // Widens the fade of the edge to this many units, centered on the edge, such as for a soft shadow.
    float softness = 0.0F;

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    static constexpr std::array<std::pair<std::string_view, Kind>, 2> kKindNames{{{"rectangle", Kind::Rectangle}, {"ellipse", Kind::Ellipse}}};
};

} // namespace haylen::graphics2d
