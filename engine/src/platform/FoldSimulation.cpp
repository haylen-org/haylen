#include "haylen/platform/FoldSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::platform {

FoldSimulation FoldSimulation::fromJson(const core::Json& value) {
    FoldSimulation simulation;
    if (value.is_string()) {
        const auto found = std::ranges::find(kPresets, value.get<std::string>(), &Preset::name);
        if (found == kPresets.end()) {
            throw std::invalid_argument("There is no simulated fold named \"" + value.get<std::string>() + "\". Use \"book\", \"tabletop\", \"flat\" or \"dualScreen\".");
        }
        simulation.preset = static_cast<std::size_t>(found - kPresets.begin());
        simulation.axis = found->axis;
        simulation.state = found->state;
        simulation.hinge = found->hinge;
        return simulation;
    }
    if (!value.is_object()) {
        throw std::invalid_argument("A simulated fold is a preset name, such as \"book\", or an object with \"axis\", \"state\" and \"hinge\".");
    }

    core::JsonValidator::requireKnownKeys(value, {"axis", "state", "hinge"}, "a simulated fold");
    const std::string axisName = value.value("axis", std::string("vertical"));
    const auto axis = std::ranges::find(Fold::kAxisNames, axisName, &std::pair<std::string_view, Fold::Axis>::first);
    if (axis == Fold::kAxisNames.end()) {
        throw std::invalid_argument("The axis of a simulated fold is \"vertical\" or \"horizontal\", not \"" + axisName + "\".");
    }
    const std::string stateName = value.value("state", std::string("flat"));
    const auto state = std::ranges::find(Fold::kStateNames, stateName, &std::pair<std::string_view, Fold::State>::first);
    if (state == Fold::kStateNames.end()) {
        throw std::invalid_argument("The state of a simulated fold is \"flat\" or \"halfOpened\", not \"" + stateName + "\".");
    }
    const core::Json hinge = value.value("hinge", core::Json(0));
    if (!hinge.is_number() || !std::isfinite(hinge.get<double>()) || hinge.get<double>() < 0.0) {
        throw std::invalid_argument("The hinge of a simulated fold is a width of at least 0 window points.");
    }
    simulation.axis = axis->second;
    simulation.state = state->second;
    simulation.hinge = hinge.get<float>();
    return simulation;
}

Fold FoldSimulation::getFold(math::Vec2 framebuffer, float dpiScale) const noexcept {
    const float width = hinge * dpiScale;
    Fold fold{.axis = axis, .state = state, .separating = state == Fold::State::HalfOpened || hinge > 0.0F, .occluding = hinge > 0.0F};
    if (axis == Fold::Axis::Vertical) {
        fold.bounds = {(framebuffer.x - width) / 2.0F, 0.0F, width, framebuffer.y};
    } else {
        fold.bounds = {0.0F, (framebuffer.y - width) / 2.0F, framebuffer.x, width};
    }
    return fold;
}

core::Json FoldSimulation::toJson() const {
    if (preset) {
        return std::string(kPresets[*preset].name);
    }
    return {{"axis", std::string(Fold::axisName(axis))}, {"state", std::string(Fold::stateName(state))}, {"hinge", core::JsonNumber::fromFloat(hinge)}};
}

} // namespace haylen::platform
