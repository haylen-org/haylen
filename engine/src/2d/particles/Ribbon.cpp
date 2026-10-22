#include "haylen/2d/particles/Ribbon.hpp"

#include <algorithm>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Stroke.hpp"
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

void Ribbon::draw(graphics2d::Renderer& renderer, std::span<const math::Vec2> points, std::span<const float> places, const Style& style, const graphics::Texture& texture, const graphics2d::DrawOrder& order) {
    if (points.size() < 2) {
        return;
    }
    const float headWidth = math::Math::lerp(style.widthStart, style.widthEnd, places[0]);
    const bool crowded = points.size() > 2 && math::Vec2::distance(points[0], points[1]) < headWidth;
    line.clear();
    widths.clear();
    tints.clear();
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (crowded && index == 1) {
            continue;
        }
        line.push_back(points[index]);
        widths.push_back(math::Math::lerp(style.widthStart, style.widthEnd, places[index]));
        tints.push_back(colorAt(style.colors, places[index]) * style.tint);
    }
    renderer.drawStroke(line, {.widths = widths, .colors = tints, .join = graphics2d::Stroke::Join::Round, .cap = graphics2d::Stroke::Cap::Round, .texture = texture}, order);
}

} // namespace haylen::particles2d
