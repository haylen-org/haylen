#include "ui/IconColor.hpp"

#include <stdexcept>
#include <string>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/PropertyReader.hpp"

namespace haylen::ui {

IconColor IconColor::fromJson(const core::Json& value, std::string_view path) {
    IconColor color;
    if (value.is_boolean() && !value.get<bool>()) {
        color.ownColors = true;
        return color;
    }
    color.role = value.is_string() ? Theme::colorFromName(value.get_ref<const std::string&>()) : std::nullopt;
    if (!color.role) {
        throw std::invalid_argument(PropertyReader::describeProperty(path) + " must name a theme color such as \"accent\" or \"textMuted\", or be \"false\".");
    }
    return color;
}

math::Color IconColor::resolve(const Context& context, math::Color fallback) const noexcept {
    if (ownColors) {
        return math::Color::white();
    }
    return role ? context.getColor(*role) : fallback;
}

} // namespace haylen::ui
