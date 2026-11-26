#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterMask.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>

namespace haylen::physics3d::jolt {

// The layers of the bodies of a world. A layer of an object holds the group of its body and the mask of the groups it meets, and the broad phase keeps the bodies that never move apart from the bodies that move, so bodies that never move never test each other.
class Layers final {
  public:
    static constexpr JPH::uint32 kStaticGroup = 1U << 0;
    static constexpr JPH::uint32 kMovingGroup = 1U << 1;

    Layers();

    Layers(const Layers&) = delete;
    Layers& operator=(const Layers&) = delete;

    [[nodiscard]] static JPH::ObjectLayer getStaticLayer() noexcept;
    [[nodiscard]] static JPH::ObjectLayer getMovingLayer() noexcept;

    [[nodiscard]] const JPH::BroadPhaseLayerInterface& getBroadPhaseLayers() const noexcept {
        return broadPhaseLayers;
    }
    [[nodiscard]] const JPH::ObjectVsBroadPhaseLayerFilter& getBroadPhaseFilter() const noexcept {
        return broadPhaseFilter;
    }
    [[nodiscard]] const JPH::ObjectLayerPairFilter& getPairFilter() const noexcept {
        return pairFilter;
    }

  private:
    static constexpr JPH::BroadPhaseLayer kStaticBroadPhase{0};
    static constexpr JPH::BroadPhaseLayer kMovingBroadPhase{1};
    static constexpr JPH::uint kBroadPhaseLayers = 2;

    JPH::BroadPhaseLayerInterfaceMask broadPhaseLayers;
    JPH::ObjectVsBroadPhaseLayerFilterMask broadPhaseFilter;
    JPH::ObjectLayerPairFilterMask pairFilter;
};

} // namespace haylen::physics3d::jolt
