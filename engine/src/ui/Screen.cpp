#include "haylen/ui/Screen.hpp"

#include <algorithm>

namespace haylen::ui {

const std::array<std::pair<std::string_view, Screen::State>, 6> Screen::kStateNames{{{"created", State::Created}, {"appearing", State::Appearing}, {"shown", State::Shown}, {"disappearing", State::Disappearing}, {"hidden", State::Hidden}, {"removed", State::Removed}}};

std::optional<Screen::State> Screen::stateFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStateNames, name, &std::pair<std::string_view, State>::first);
    return found != kStateNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Screen::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

void Screen::appear(core::Engine&) {}

void Screen::appeared(core::Engine&) {}

void Screen::disappear(core::Engine&) {}

void Screen::disappeared(core::Engine&) {}

void Screen::update(core::Engine&, float) {}

void Screen::render(core::Engine&) {}

bool Screen::back(core::Engine&) {
    return false;
}

void Screen::remove(core::Engine&) {}

void Screen::placed(core::Engine&) {}

} // namespace haylen::ui
