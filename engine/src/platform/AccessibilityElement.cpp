#include "haylen/platform/AccessibilityElement.hpp"

#include <algorithm>

namespace haylen::platform {

std::string_view AccessibilityElement::roleName(Role value) noexcept {
    return std::ranges::find(kRoleNames, value, &std::pair<std::string_view, Role>::second)->first;
}

std::optional<AccessibilityElement::Role> AccessibilityElement::roleFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kRoleNames, name, &std::pair<std::string_view, Role>::first);
    return found != kRoleNames.end() ? std::optional(found->second) : std::nullopt;
}

std::optional<AccessibilityElement::Action> AccessibilityElement::actionFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kActionNames, name, &std::pair<std::string_view, Action>::first);
    return found != kActionNames.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace haylen::platform
