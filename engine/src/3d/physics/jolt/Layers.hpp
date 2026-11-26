#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "haylen/3d/physics/CollisionFilter.hpp"

namespace haylen::physics3d::jolt {

// The layers of the bodies of a world. The object layer of a body of Jolt is the index of its collision filter and its part of the broad phase in a table, which grows as bodies take filters it does not hold yet, so every filter of 32 categories, 32 bits of mask and a group fits. The broad phase keeps bodies that never move, bodies that move and sensors apart, so bodies that never move never test each other and a sensor never tests another. Only the thread that steps the world adds to the table, between steps, so the threads of a step read it without a lock.
class Layers final {
  public:
    enum class Part : std::uint8_t {
        Static,
        Moving,
        Sensor,
    };

    Layers();

    Layers(const Layers&) = delete;
    Layers& operator=(const Layers&) = delete;

    // Returns the layer of the filter in the part, which the table adds when it is new.
    [[nodiscard]] JPH::ObjectLayer getLayer(const CollisionFilter& filter, Part part);

    [[nodiscard]] const CollisionFilter& getFilter(JPH::ObjectLayer layer) const noexcept {
        return entries[layer].filter;
    }
    [[nodiscard]] Part getPart(JPH::ObjectLayer layer) const noexcept {
        return entries[layer].part;
    }

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
    static constexpr JPH::uint kParts = 3;

    struct Entry {
        CollisionFilter filter;
        Part part = Part::Moving;
    };

    // The part of the broad phase that a layer belongs to.
    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
      public:
        explicit BroadPhaseLayers(const Layers& owner) noexcept : layers(owner) {}

        [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;
        [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

      private:
        const Layers& layers;
    };

    // A body that moves meets every part, a sensor meets what never moves and what moves, and a body that never moves meets only what moves.
    class BroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
      public:
        explicit BroadPhaseFilter(const Layers& owner) noexcept : layers(owner) {}

        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer part) const override;

      private:
        const Layers& layers;
    };

    // Two layers meet when their filters collide.
    class PairFilter final : public JPH::ObjectLayerPairFilter {
      public:
        explicit PairFilter(const Layers& owner) noexcept : layers(owner) {}

        [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override;

      private:
        const Layers& layers;
    };

    using Key = std::tuple<std::uint32_t, std::uint32_t, int, Part>;

    std::vector<Entry> entries;
    std::map<Key, JPH::ObjectLayer> index;
    BroadPhaseLayers broadPhaseLayers{*this};
    BroadPhaseFilter broadPhaseFilter{*this};
    PairFilter pairFilter{*this};
};

} // namespace haylen::physics3d::jolt
