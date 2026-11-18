#include "haylen/graphics/Gradient.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::graphics {

Gradient::Gradient(std::vector<Stop> gradientStops, Kind gradientKind) : stops(std::move(gradientStops)), kind(gradientKind) {
    if (stops.empty()) {
        throw std::invalid_argument("A gradient needs at least one color stop.");
    }

    const bool ordered = std::ranges::all_of(stops, [](const Stop& stop) { return stop.offset >= 0.0F && stop.offset <= 1.0F; }) && std::ranges::is_sorted(stops, {}, &Stop::offset);
    if (!ordered) {
        throw std::invalid_argument("The stops of a gradient need offsets from 0 to 1 in increasing order.");
    }
}

Gradient Gradient::fromColors(std::span<const math::Color> colors, Kind gradientKind) {
    std::vector<Stop> spread;
    spread.reserve(colors.size());
    for (std::size_t index = 0; index < colors.size(); ++index) {
        spread.push_back({.offset = colors.size() > 1 ? static_cast<float>(index) / static_cast<float>(colors.size() - 1) : 0.0F, .color = colors[index]});
    }

    return Gradient(std::move(spread), gradientKind);
}

std::optional<Gradient::Kind> Gradient::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional<Kind>(found->second) : std::nullopt;
}

std::string_view Gradient::kindName(Kind value) noexcept {
    const auto found = std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second);
    return found != kKindNames.end() ? found->first : std::string_view{};
}

void Gradient::setAngle(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("A gradient needs a finite angle.");
    }

    angle = value;
}

void Gradient::setCenter(math::Vec2 value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("A gradient needs a center of finite coordinates.");
    }

    center = value;
}

void Gradient::setRadius(math::Vec2 value) {
    if (!(value.x > 0.0F && value.y > 0.0F) || !std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("A radial gradient needs a finite radius above 0 on both axes.");
    }

    radius = value;
}

// A hard stop, two stops at one offset, switches to the later color there.
math::Color Gradient::getColor(float at) const noexcept {
    if (!(at > stops.front().offset)) {
        return stops.front().color;
    }

    if (at >= stops.back().offset) {
        return stops.back().color;
    }

    const auto next = std::ranges::upper_bound(stops, at, {}, &Stop::offset);
    const Stop& to = *next;
    const Stop& from = *(next - 1);
    const float share = (at - from.offset) / (to.offset - from.offset);
    const float alpha = from.color.a + (to.color.a - from.color.a) * share;
    if (alpha <= 0.0F) {
        return math::Color::transparent();
    }

    const auto blend = [&](float first, float second) { return (first * from.color.a + (second * to.color.a - first * from.color.a) * share) / alpha; };
    return {blend(from.color.r, to.color.r), blend(from.color.g, to.color.g), blend(from.color.b, to.color.b), alpha};
}

void Gradient::paint(Image& image, const Image::Area& area) const {
    // The ends of the area reckon in 64 bits, so an area far outside the image never wraps around onto it.
    const int left = std::max(area.x, 0);
    const int top = std::max(area.y, 0);
    const auto right = static_cast<int>(std::min<std::int64_t>(std::int64_t{area.x} + area.width, image.getWidth()));
    const auto bottom = static_cast<int>(std::min<std::int64_t>(std::int64_t{area.y} + area.height, image.getHeight()));
    if (left >= right || top >= bottom) {
        return;
    }

    const math::Vec2 size{static_cast<float>(area.width), static_cast<float>(area.height)};
    const std::span<std::uint8_t> pixels = image.getWritablePixels();
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            const auto across = static_cast<float>(std::int64_t{column} - area.x);
            const auto down = static_cast<float>(std::int64_t{row} - area.y);
            const math::Color color = getColor(positionOf({across + 0.5F, down + 0.5F}, size));
            const float offset = dithered ? (kDither[static_cast<std::size_t>((row & 3) * 4 + (column & 3))] + 0.5F) / 16.0F : 0.5F;
            std::uint8_t* pixel = &pixels[static_cast<std::size_t>(row * image.getWidth() + column) * 4U];
            const std::array<float, 4> channels{color.r, color.g, color.b, color.a};
            for (std::size_t channel = 0; channel < 4; ++channel) {
                pixel[channel] = static_cast<std::uint8_t>(std::clamp(std::floor(channels[channel] * 255.0F + offset), 0.0F, 255.0F));
            }
        }
    }
}

// A linear gradient reaches 0 and 1 at the two corners of the area furthest along its direction, a radial one reaches 1 at its radius, and a conic one goes once around.
float Gradient::positionOf(math::Vec2 point, math::Vec2 size) const noexcept {
    switch (kind) {
    case Kind::Linear: {
        const math::Vec2 direction{std::cos(angle), std::sin(angle)};
        const float extent = std::fabs(size.x * direction.x) + std::fabs(size.y * direction.y);
        return math::Vec2::dot(point - size * 0.5F, direction) / extent + 0.5F;
    }
    case Kind::Radial: {
        const math::Vec2 offset = point - center * size;
        return math::Vec2{offset.x / (radius.x * size.x), offset.y / (radius.y * size.y)}.getLength();
    }
    case Kind::Conic:
        break;
    }
    const math::Vec2 offset = point - center * size;
    const float turn = (std::atan2(offset.y, offset.x) - angle) / math::Math::kTau;
    return turn - std::floor(turn);
}

} // namespace haylen::graphics
