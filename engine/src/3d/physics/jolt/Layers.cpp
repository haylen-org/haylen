#include "3d/physics/jolt/Layers.hpp"

namespace haylen::physics3d::jolt {

Layers::Layers() = default;

JPH::ObjectLayer Layers::getLayer(const CollisionFilter& filter, Part part) {
    const Key key{filter.category, filter.mask, filter.group, part};
    const auto found = index.find(key);
    if (found != index.end()) {
        return found->second;
    }

    const auto layer = static_cast<JPH::ObjectLayer>(entries.size());
    entries.push_back({.filter = filter, .part = part});
    index.emplace(key, layer);
    return layer;
}

JPH::uint Layers::BroadPhaseLayers::GetNumBroadPhaseLayers() const {
    return kParts;
}

JPH::BroadPhaseLayer Layers::BroadPhaseLayers::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
    return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layers.getPart(layer)));
}

bool Layers::BroadPhaseFilter::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer part) const {
    const auto other = static_cast<Part>(part.GetValue());
    switch (layers.getPart(layer)) {
    case Part::Static:
        return other == Part::Moving;
    case Part::Sensor:
        return other != Part::Sensor;
    case Part::Moving:
        break;
    }

    return true;
}

bool Layers::PairFilter::ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const {
    return layers.getFilter(first).canCollide(layers.getFilter(second));
}

} // namespace haylen::physics3d::jolt
