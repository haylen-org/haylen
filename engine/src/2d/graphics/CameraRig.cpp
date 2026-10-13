#include "haylen/2d/graphics/CameraRig.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace haylen::graphics2d {

CameraRig::CameraRig(Options value) {
    setOptions(value);
}

void CameraRig::setOptions(const Options& value) {
    if (!(value.blendTime >= 0.0F) || !(value.zoom.x > 0.0F && value.zoom.y > 0.0F)) {
        throw std::invalid_argument("A camera rig needs a blend time of zero or more and a positive zoom.");
    }
    options = value;
}

void CameraRig::addZone(Zone zone) {
    if (zone.name.empty() || !(zone.area.width > 0.0F && zone.area.height > 0.0F)) {
        throw std::invalid_argument("A camera zone needs a name and an area of a positive size.");
    }
    if (zone.zoom && !(zone.zoom->x > 0.0F && zone.zoom->y > 0.0F)) {
        throw std::invalid_argument("The camera zone \"" + zone.name + "\" needs a positive zoom.");
    }
    const auto found = std::ranges::find(zones, zone.name, &Zone::name);
    if (found != zones.end()) {
        *found = std::move(zone);
        return;
    }
    zones.push_back(std::move(zone));
}

void CameraRig::removeZone(std::string_view name) {
    std::erase_if(zones, [name](const Zone& zone) { return zone.name == name; });
}

void CameraRig::clearZones() noexcept {
    zones.clear();
}

void CameraRig::setRail(std::vector<math::Vec2> points) {
    if (points.size() == 1) {
        throw std::invalid_argument("A camera rail needs two points or more, or none to take it away.");
    }
    rail = std::move(points);
}

math::Vec2 CameraRig::project(math::Vec2 point) const noexcept {
    if (rail.empty()) {
        return point;
    }
    math::Vec2 nearest = rail.front();
    float best = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index + 1 < rail.size(); ++index) {
        const math::Vec2 start = rail[index];
        const math::Vec2 along = rail[index + 1] - start;
        const float length = along.getLengthSquared();
        const float share = length > 0.0F ? std::clamp(math::Vec2::dot(point - start, along) / length, 0.0F, 1.0F) : 0.0F;
        const math::Vec2 candidate = start + along * share;
        const float distance = math::Vec2::distanceSquared(candidate, point);
        if (distance < best) {
            best = distance;
            nearest = candidate;
        }
    }
    return nearest;
}

// The zone of highest priority that holds the target wins, and between zones of equal priority the zone the target is in already stays.
std::optional<std::size_t> CameraRig::pick(math::Vec2 target) const noexcept {
    std::optional<std::size_t> chosen;
    for (std::size_t index = 0; index < zones.size(); ++index) {
        const Zone& zone = zones[index];
        if (!zone.area.contains(target)) {
            continue;
        }
        const bool higher = !chosen || zone.priority > zones[*chosen].priority;
        const bool holds = chosen && zone.priority == zones[*chosen].priority && current == zone.name;
        if (higher || holds) {
            chosen = index;
        }
    }
    return chosen;
}

const CameraRig::Zone* CameraRig::getZone() const noexcept {
    if (!current) {
        return nullptr;
    }
    const auto found = std::ranges::find(zones, *current, &Zone::name);
    return found != zones.end() ? &*found : nullptr;
}

CameraRig::Framing CameraRig::framingOf(const Zone* zone) const {
    if (zone == nullptr) {
        return {.limits = options.limits, .zoom = options.zoom};
    }
    return {.limits = zone->limits.value_or(zone->area), .zoom = zone->zoom.value_or(options.zoom), .offset = zone->offset};
}

CameraRig::Framing CameraRig::mix(const Framing& start, const Framing& end, float amount) {
    Framing result{.zoom = math::Vec2::lerp(start.zoom, end.zoom, amount), .offset = math::Vec2::lerp(start.offset, end.offset, amount)};
    if (start.limits && end.limits) {
        const math::Vec2 low = math::Vec2::lerp(start.limits->getMin(), end.limits->getMin(), amount);
        const math::Vec2 high = math::Vec2::lerp(start.limits->getMax(), end.limits->getMax(), amount);
        result.limits = math::Rect::fromMinMax(low, high);
    } else {
        result.limits = end.limits;
    }
    return result;
}

void CameraRig::update(Camera& camera, math::Vec2 target, float deltaSeconds, const math::Rect& screen) {
    const std::optional<std::size_t> chosen = pick(target);
    const std::optional<std::string> name = chosen ? std::optional<std::string>(zones[*chosen].name) : std::nullopt;
    if (!started) {
        started = true;
        current = name;
        progress = 1.0F;
    } else if (name != current) {
        from = applied;
        current = name;
        progress = 0.0F;
    }
    progress = options.blendTime > 0.0F ? std::min(1.0F, progress + deltaSeconds / options.blendTime) : 1.0F;
    const float eased = progress * progress * (3.0F - 2.0F * progress);
    applied = progress >= 1.0F ? framingOf(getZone()) : mix(from, framingOf(getZone()), eased);

    camera.limits = applied.limits;
    camera.setZoom(applied.zoom);
    camera.offset = applied.offset;
    camera.follow(project(target), deltaSeconds, screen);
}

} // namespace haylen::graphics2d
