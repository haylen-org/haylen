#include "haylen/2d/tiled/Properties.hpp"

#include <algorithm>

namespace haylen::tiled {

const Property* Properties::find(std::string_view name) const noexcept {
    const auto found = std::find_if(items.begin(), items.end(), [name](const Property& property) { return property.name == name; });
    return found == items.end() ? nullptr : &*found;
}

bool Properties::getBool(std::string_view name, bool fallback) const {
    const Property* property = find(name);
    return property != nullptr ? property->value.get<bool>() : fallback;
}

double Properties::getNumber(std::string_view name, double fallback) const {
    const Property* property = find(name);
    return property != nullptr ? property->value.get<double>() : fallback;
}

std::string Properties::getString(std::string_view name, std::string_view fallback) const {
    const Property* property = find(name);
    return property != nullptr ? property->value.get<std::string>() : std::string(fallback);
}

Properties Properties::merged(const Properties& overrides) const {
    std::vector<Property> result = items;
    for (const Property& property : overrides.getItems()) {
        const auto found = std::find_if(result.begin(), result.end(), [&property](const Property& item) { return item.name == property.name; });
        if (found != result.end()) {
            *found = property;
        } else {
            result.push_back(property);
        }
    }
    return Properties(std::move(result));
}

} // namespace haylen::tiled
