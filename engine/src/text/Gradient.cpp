#include "haylen/text/Gradient.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::text {

Gradient::Gradient(std::span<const Stop> gradientStops, float gradientAngle) {
    if (gradientStops.empty()) {
        throw std::invalid_argument("A gradient needs at least one color stop.");
    }
    if (gradientStops.size() > kMaxStops) {
        throw std::invalid_argument("A text gradient holds at most 4 color stops.");
    }

    const bool ordered = std::ranges::all_of(gradientStops, [](const Stop& stop) { return stop.offset >= 0.0F && stop.offset <= 1.0F; }) && std::ranges::is_sorted(gradientStops, {}, &Stop::offset);
    if (!ordered) {
        throw std::invalid_argument("The stops of a gradient need offsets from 0 to 1 in increasing order.");
    }

    std::ranges::copy(gradientStops, stops.begin());
    count = gradientStops.size();
    setAngle(gradientAngle);
}

Gradient Gradient::fromColors(std::span<const math::Color> colors, float gradientAngle) {
    if (colors.size() > kMaxStops) {
        throw std::invalid_argument("A text gradient holds at most 4 color stops.");
    }

    std::array<Stop, kMaxStops> spread{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        spread[index] = {.offset = colors.size() > 1 ? static_cast<float>(index) / static_cast<float>(colors.size() - 1) : 0.0F, .color = colors[index]};
    }
    return Gradient(std::span(spread.data(), colors.size()), gradientAngle);
}

void Gradient::setAngle(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("The angle of a text gradient must be a finite number of radians.");
    }
    angle = value;
}

} // namespace haylen::text
