#include "haylen/platform/SafeAreaSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/JsonNumber.hpp"

namespace haylen::platform {

float SafeAreaSimulation::readSide(const core::Json& value) {
    if (!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() < 0.0) {
        throw std::invalid_argument("A simulated safe area needs non-negative insets.");
    }
    return value.get<float>();
}

SafeAreaSimulation SafeAreaSimulation::fromJson(const core::Json& value) {
    SafeAreaSimulation simulation;
    if (value.is_string()) {
        const auto found = std::ranges::find(kDevices, value.get<std::string>(), &Device::name);
        if (found == kDevices.end()) {
            throw std::invalid_argument("There is no simulated device named \"" + value.get<std::string>() + "\".");
        }
        simulation.device = static_cast<std::size_t>(found - kDevices.begin());
        return simulation;
    }

    std::vector<float> sides;
    if (value.is_number()) {
        sides.assign(4, readSide(value));
    } else if (value.is_array() && (value.size() == 2 || value.size() == 4)) {
        for (const core::Json& side : value) {
            sides.push_back(readSide(side));
        }
    } else {
        throw std::invalid_argument("A simulated safe area is a device name or one, two or four insets.");
    }
    simulation.insets = sides.size() == 2 ? math::Insets{.left = sides[1], .top = sides[0], .right = sides[1], .bottom = sides[0]} : math::Insets{.left = sides[3], .top = sides[0], .right = sides[1], .bottom = sides[2]};
    return simulation;
}

math::Insets SafeAreaSimulation::getInsets(math::Vec2 framebuffer, float dpiScale) const noexcept {
    if (!device) {
        return {.left = insets.left * dpiScale, .top = insets.top * dpiScale, .right = insets.right * dpiScale, .bottom = insets.bottom * dpiScale};
    }

    // The device turns with the window, so its insets keep their share of the screen along each axis.
    const Device& simulated = kDevices[*device];
    const bool landscape = framebuffer.x > framebuffer.y;
    const math::Vec2 screen = landscape ? math::Vec2{simulated.screen.y, simulated.screen.x} : simulated.screen;
    const math::Insets& sides = landscape ? simulated.landscape : simulated.portrait;
    const math::Vec2 scale = framebuffer / screen;
    return {.left = sides.left * scale.x, .top = sides.top * scale.y, .right = sides.right * scale.x, .bottom = sides.bottom * scale.y};
}

core::Json SafeAreaSimulation::toJson() const {
    if (device) {
        return std::string(kDevices[*device].name);
    }
    return core::Json::array({core::JsonNumber::fromFloat(insets.top), core::JsonNumber::fromFloat(insets.right), core::JsonNumber::fromFloat(insets.bottom), core::JsonNumber::fromFloat(insets.left)});
}

} // namespace haylen::platform
