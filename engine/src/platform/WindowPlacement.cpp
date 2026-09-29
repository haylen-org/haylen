#include "haylen/platform/WindowPlacement.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::platform {

template <typename T, std::size_t Size> T WindowPlacement::readName(const core::Json& object, const char* key, const std::array<std::pair<std::string_view, T>, Size>& names, T fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const core::Json& value = object.at(key);
    if (value.is_string()) {
        const auto found = std::ranges::find(names, value.get<std::string>(), &std::pair<std::string_view, T>::first);
        if (found != names.end()) {
            return found->second;
        }
    }
    throw std::invalid_argument(std::string("A window position has an unknown ") + key + ": " + value.dump() + ".");
}

template <typename T, std::size_t Size> std::string_view WindowPlacement::getName(const std::array<std::pair<std::string_view, T>, Size>& names, T value) {
    return std::ranges::find(names, value, &std::pair<std::string_view, T>::second)->first;
}

float WindowPlacement::readNumber(const core::Json& value, const char* key) {
    if (!value.is_number() || !std::isfinite(value.get<double>())) {
        throw std::invalid_argument(std::string("A window position needs a number for ") + key + ".");
    }
    return value.get<float>();
}

WindowPlacement WindowPlacement::fromJson(const core::Json& value) {
    WindowPlacement placement;
    if (value == "center") {
        return placement;
    }
    if (!value.is_object()) {
        throw std::invalid_argument("A window position is \"center\", a point with x and y, or an object with an anchor.");
    }
    if (value.contains("x") || value.contains("y")) {
        core::JsonValidator::requireKnownKeys(value, {"x", "y"}, "window position");
        placement.point = math::Vec2{readNumber(value.value("x", core::Json()), "x"), readNumber(value.value("y", core::Json()), "y")};
        return placement;
    }

    core::JsonValidator::requireKnownKeys(value, {"anchor", "area", "monitor", "offset", "fill"}, "window position");
    placement.anchor = readName(value, "anchor", kAnchors, Anchor::Center);
    placement.area = readName(value, "area", kAreas, Area::Work);
    placement.fill = readName(value, "fill", kFills, Fill::None);
    if (value.contains("monitor")) {
        const core::Json& number = value.at("monitor");
        if (number.is_number_integer() && number.get<long long>() >= 1) {
            placement.monitor = number.get<std::size_t>();
        } else if (number != "primary") {
            throw std::invalid_argument("A window position names its monitor with \"primary\" or a number from 1.");
        }
    }
    if (value.contains("offset")) {
        const core::Json& shift = value.at("offset");
        if (!shift.is_array() || shift.size() != 2) {
            throw std::invalid_argument("A window position offset is a pair of numbers.");
        }
        placement.offset = {readNumber(shift[0], "offset"), readNumber(shift[1], "offset")};
    }
    return placement;
}

core::Json WindowPlacement::toJson() const {
    if (point) {
        return {{"x", core::JsonNumber::fromFloat(point->x)}, {"y", core::JsonNumber::fromFloat(point->y)}};
    }
    const core::Json target = monitor == 0 ? core::Json("primary") : core::Json(monitor);
    return {{"anchor", getName(kAnchors, anchor)}, {"area", getName(kAreas, area)}, {"monitor", target}, {"offset", {core::JsonNumber::fromFloat(offset.x), core::JsonNumber::fromFloat(offset.y)}}, {"fill", getName(kFills, fill)}};
}

const Monitor& WindowPlacement::findMonitor(std::span<const Monitor> monitors, std::size_t number) {
    if (number >= 1 && number <= monitors.size()) {
        return monitors[number - 1];
    }
    const auto primary = std::ranges::find_if(monitors, &Monitor::primary);
    return primary != monitors.end() ? *primary : monitors.front();
}

math::Rect WindowPlacement::resolve(std::span<const Monitor> monitors, math::Vec2 size) const {
    if (point) {
        return {point->x, point->y, size.x, size.y};
    }

    const Monitor& target = findMonitor(monitors, monitor);
    const math::Rect& region = area == Area::Work ? target.workArea : target.bounds;
    const float width = fill == Fill::Width || fill == Fill::Both ? region.width : size.x;
    const float height = fill == Fill::Height || fill == Fill::Both ? region.height : size.y;

    float x = region.x + (region.width - width) * 0.5F;
    if (anchor == Anchor::Left || anchor == Anchor::TopLeft || anchor == Anchor::BottomLeft) {
        x = region.x;
    } else if (anchor == Anchor::Right || anchor == Anchor::TopRight || anchor == Anchor::BottomRight) {
        x = region.getRight() - width;
    }
    float y = region.y + (region.height - height) * 0.5F;
    if (anchor == Anchor::Top || anchor == Anchor::TopLeft || anchor == Anchor::TopRight) {
        y = region.y;
    } else if (anchor == Anchor::Bottom || anchor == Anchor::BottomLeft || anchor == Anchor::BottomRight) {
        y = region.getBottom() - height;
    }
    return {std::round(x + offset.x), std::round(y + offset.y), width, height};
}

} // namespace haylen::platform
