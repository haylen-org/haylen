#include "3d/physics/jolt/Layers.hpp"

namespace haylen::physics3d::jolt {

Layers::Layers() : broadPhaseLayers(kBroadPhaseLayers), broadPhaseFilter(broadPhaseLayers) {
    broadPhaseLayers.ConfigureLayer(kStaticBroadPhase, kStaticGroup, 0);
    broadPhaseLayers.ConfigureLayer(kMovingBroadPhase, kMovingGroup, 0);
}

JPH::ObjectLayer Layers::getStaticLayer() noexcept {
    return JPH::ObjectLayerPairFilterMask::sGetObjectLayer(kStaticGroup, kMovingGroup);
}

JPH::ObjectLayer Layers::getMovingLayer() noexcept {
    return JPH::ObjectLayerPairFilterMask::sGetObjectLayer(kMovingGroup, kStaticGroup | kMovingGroup);
}

} // namespace haylen::physics3d::jolt
