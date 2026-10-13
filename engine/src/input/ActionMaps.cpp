#include "haylen/input/ActionMaps.hpp"

#include <utility>

namespace haylen::input {

ActionMaps::ActionMaps() : defaultMap(std::make_shared<ActionMap>()) {}

std::shared_ptr<ActionMap> ActionMaps::create(const ActionMap::Devices& devices) {
    auto map = std::make_shared<ActionMap>();
    map->setDevices(devices);
    map->setCapture(capture);
    created.push_back(map);
    return map;
}

void ActionMaps::setCapture(const ActionMap::Capture& value) noexcept {
    capture = value;
    defaultMap->setCapture(value);
    for (const std::weak_ptr<ActionMap>& map : created) {
        if (const std::shared_ptr<ActionMap> live = map.lock()) {
            live->setCapture(value);
        }
    }
}

void ActionMaps::update(const Input& input, const VirtualInput& virtualInput, bool blocked) {
    std::erase_if(created, [](const std::weak_ptr<ActionMap>& map) { return map.expired(); });
    defaultMap->update(input, virtualInput, blocked);
    for (const std::weak_ptr<ActionMap>& map : created) {
        if (const std::shared_ptr<ActionMap> live = map.lock()) {
            live->update(input, virtualInput, blocked);
        }
    }
}

void ActionMaps::beginStep(float stepSeconds) noexcept {
    defaultMap->beginStep(stepSeconds);
    for (const std::weak_ptr<ActionMap>& map : created) {
        if (const std::shared_ptr<ActionMap> live = map.lock()) {
            live->beginStep(stepSeconds);
        }
    }
}

} // namespace haylen::input
