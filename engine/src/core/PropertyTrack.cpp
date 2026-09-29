#include "haylen/core/PropertyTrack.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace haylen::core {

PropertyTrack::PropertyTrack(const void* owner, std::vector<std::string> names, Getter read, Setter write, TweenProperty value, std::function<bool()> alive) : target(owner), fields(std::move(names)), getter(std::move(read)), setter(std::move(write)), property(std::move(value)), aliveCheck(std::move(alive)) {
    if (!getter || !setter) {
        throw std::invalid_argument("A property track needs a getter and a setter.");
    }
}

void PropertyTrack::begin() {
    property.begin(getter());
}

void PropertyTrack::render(float progress, int loops) {
    if (!released) {
        setter(property.evaluate(progress, loops));
    }
}

bool PropertyTrack::isAlive() const {
    return !aliveCheck || aliveCheck();
}

float PropertyTrack::getDistance() const {
    return property.getDistance();
}

std::vector<std::string> PropertyTrack::getFields() const {
    if (released) {
        return {};
    }
    return fields;
}

bool PropertyTrack::release(std::string_view name) {
    if (std::find(fields.begin(), fields.end(), name) != fields.end()) {
        released = true;
    }
    return !released;
}

} // namespace haylen::core
