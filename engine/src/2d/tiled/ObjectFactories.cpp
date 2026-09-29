#include "haylen/2d/tiled/ObjectFactories.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/2d/tiled/MapRenderer.hpp"

namespace haylen::tiled {

void ObjectFactories::add(std::string type, Factory factory) {
    if (type.empty() || !factory) {
        throw std::invalid_argument("A Tiled object factory needs a class name and a function.");
    }
    if (factories.contains(type)) {
        throw std::invalid_argument("The Tiled object class " + type + " already has a factory.");
    }
    factories.emplace(std::move(type), std::move(factory));
}

void ObjectFactories::remove(std::string_view type) {
    if (const auto found = factories.find(type); found != factories.end()) {
        factories.erase(found);
    }
}

bool ObjectFactories::has(std::string_view type) const noexcept {
    return factories.find(type) != factories.end();
}

std::size_t ObjectFactories::spawn(const MapRenderer& map, std::string_view layer) const {
    std::size_t spawned = 0;
    // clang-format off
    map.forEachObject(layer, [&](const Object& object, math::Vec2 position) {
        // A factory may add or remove factories, its own too, so it runs from a copy.
        if (const auto found = factories.find(object.type); found != factories.end()) {
            const Factory factory = found->second;
            factory(object, position);
            ++spawned;
        }
    });
    // clang-format on
    return spawned;
}

} // namespace haylen::tiled
