#include "haylen/2d/graphics/Shape.hpp"

#include <algorithm>

namespace haylen::graphics2d {

std::optional<Shape::Kind> Shape::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional<Kind>(found->second) : std::nullopt;
}

std::string_view Shape::kindName(Kind value) noexcept {
    const auto found = std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second);
    return found != kKindNames.end() ? found->first : std::string_view{};
}

} // namespace haylen::graphics2d
