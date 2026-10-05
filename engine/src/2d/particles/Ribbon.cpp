#include "haylen/2d/particles/Ribbon.hpp"

#include <algorithm>

#include "haylen/math/Math.hpp"

namespace haylen::particles2d {

math::Color Ribbon::colorAt(std::span<const math::Color> colors, float place) noexcept {
    if (colors.empty()) {
        return math::Color::white();
    }
    if (colors.size() == 1) {
        return colors.front();
    }
    const float scaled = math::Math::saturate(place) * static_cast<float>(colors.size() - 1);
    const auto index = std::min(static_cast<std::size_t>(scaled), colors.size() - 2);
    return math::Color::lerp(colors[index], colors[index + 1], scaled - static_cast<float>(index));
}

// Every point takes the direction of the line through its neighbours, so the ribbon bends smoothly, and a point on top of its neighbour keeps the direction before it.
void Ribbon::append(std::span<const math::Vec2> points, std::span<const float> places, const Style& style, std::vector<graphics2d::MeshVertex>& vertices, std::vector<std::uint32_t>& indices) {
    if (points.size() < 2) {
        return;
    }

    const auto base = static_cast<std::uint32_t>(vertices.size());
    math::Vec2 normal{0.0F, 1.0F};
    for (std::size_t index = 0; index < points.size(); ++index) {
        const math::Vec2 before = points[index == 0 ? 0 : index - 1];
        const math::Vec2 after = points[std::min(index + 1, points.size() - 1)];
        const math::Vec2 along = before - after;
        if (!along.isZero()) {
            normal = along.getNormalized().getPerpendicular();
        }

        const float place = places[index];
        const float half = math::Math::lerp(style.widthStart, style.widthEnd, place) * 0.5F;
        const math::Color color = colorAt(style.colors, place) * style.tint;
        vertices.push_back({.position = points[index] + normal * half, .uv = {place, 0.0F}, .color = color});
        vertices.push_back({.position = points[index] - normal * half, .uv = {place, 1.0F}, .color = color});
    }

    for (std::uint32_t segment = 0; segment + 1 < static_cast<std::uint32_t>(points.size()); ++segment) {
        const std::uint32_t first = base + segment * 2;
        indices.insert(indices.end(), {first, first + 1, first + 3, first, first + 3, first + 2});
    }
}

} // namespace haylen::particles2d
