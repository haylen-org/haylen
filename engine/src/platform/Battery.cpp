#include "haylen/platform/Battery.hpp"

#include <algorithm>

#include "haylen/core/JsonNumber.hpp"

namespace haylen::platform {

const std::array<std::pair<std::string_view, Battery::State>, 5> Battery::kStateNames{{{"unknown", State::Unknown}, {"charging", State::Charging}, {"discharging", State::Discharging}, {"full", State::Full}, {"none", State::None}}};

core::Json Battery::toJson() const {
    core::Json json{{"charging", charging}, {"state", stateName(state)}};
    if (level) {
        json["level"] = core::JsonNumber::fromFloat(*level);
    }
    return json;
}

std::string_view Battery::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

} // namespace haylen::platform
