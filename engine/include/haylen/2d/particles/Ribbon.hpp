#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::particles2d {

// Builds the mesh of a ribbon through a line of points, from its head to its tail, as trails draw them.
class Ribbon final {
  public:
    // The width and the colors go from the head to the tail, the colors spread evenly and multiplied by the tint, and the texture runs along the ribbon from the head.
    struct Style {
        float widthStart = 6.0F;
        float widthEnd = 0.0F;
        std::span<const math::Color> colors;
        math::Color tint = math::Color::white();
    };

    // Appends the triangles of a ribbon through the points, the head first, where `places` gives each point its place along the ribbon from 0 at the head to 1 at the tail. A ribbon needs two points.
    static void append(std::span<const math::Vec2> points, std::span<const float> places, const Style& style, std::vector<graphics2d::MeshVertex>& vertices, std::vector<std::uint32_t>& indices);

    // Returns the color at a place of evenly spread colors, blended between neighbours.
    [[nodiscard]] static math::Color colorAt(std::span<const math::Color> colors, float place) noexcept;
};

} // namespace haylen::particles2d
