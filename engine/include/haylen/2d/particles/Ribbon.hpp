#pragma once

#include <span>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::particles2d {

// Draws ribbons through lines of points, from their head to their tail, as trails draw them. A ribbon is a stroke of the renderer with round ends and joins, so its edges fade over one pixel at any width and zoom, and it keeps the widths and colors of its points between draws, so drawing allocates nothing once they have grown.
class Ribbon final {
  public:
    // The width and the colors go from the head to the tail, the colors spread evenly and multiplied by the tint, and the texture stretches along the ribbon from the head.
    struct Style {
        float widthStart = 6.0F;
        float widthEnd = 0.0F;
        std::span<const math::Color> colors;
        math::Color tint = math::Color::white();
    };

    // Draws a ribbon through the points, the head first, where `places` gives each point its place along the ribbon from 0 at the head to 1 at the tail. A ribbon needs two points.
    void draw(graphics2d::Renderer& renderer, std::span<const math::Vec2> points, std::span<const float> places, const Style& style, const graphics::Texture& texture, const graphics2d::DrawOrder& order);

    // Returns the color at a place of evenly spread colors, blended between neighbours.
    [[nodiscard]] static math::Color colorAt(std::span<const math::Color> colors, float place) noexcept;

  private:
    std::vector<float> widths;
    std::vector<math::Color> tints;
};

} // namespace haylen::particles2d
