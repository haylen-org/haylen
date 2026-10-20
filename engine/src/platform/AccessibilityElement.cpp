#include "haylen/platform/AccessibilityElement.hpp"

#include <algorithm>

#include "haylen/core/JsonNumber.hpp"

namespace haylen::platform {

core::Json AccessibilityElement::toJson() const {
    return {
        {"id", id}, {"parent", parent}, {"role", roleName(role)}, {"label", label}, {"value", value}, {"hint", hint}, {"bounds", {core::JsonNumber::fromFloat(bounds.x), core::JsonNumber::fromFloat(bounds.y), core::JsonNumber::fromFloat(bounds.width), core::JsonNumber::fromFloat(bounds.height)}}, {"states", states}, {"actions", actions},
    };
}

std::string AccessibilityElement::writeTree(std::span<const AccessibilityElement> elements, std::uint64_t focused) {
    core::Json tree{{"focused", focused}, {"elements", core::Json::array()}};
    for (const AccessibilityElement& element : elements) {
        tree["elements"].push_back(element.toJson());
    }
    return tree.dump(-1, ' ', false, core::Json::error_handler_t::replace);
}

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
