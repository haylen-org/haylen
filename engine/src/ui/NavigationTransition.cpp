#include "haylen/ui/NavigationTransition.hpp"

#include <algorithm>

namespace haylen::ui {

// Motion across the screen turns into a cross fade, which keeps the change visible without movement.
NavigationTransition NavigationTransition::reduced(bool reducedMotion) const {
    NavigationTransition result = *this;
    if (reducedMotion && (effect == Effect::Slide || effect == Effect::Rise)) {
        result.effect = Effect::Fade;
    }

    return result;
}

std::optional<NavigationTransition::Effect> NavigationTransition::effectFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kEffectNames, name, &std::pair<std::string_view, Effect>::first);
    return found != kEffectNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view NavigationTransition::effectName(Effect value) noexcept {
    return std::ranges::find(kEffectNames, value, &std::pair<std::string_view, Effect>::second)->first;
}

} // namespace haylen::ui
