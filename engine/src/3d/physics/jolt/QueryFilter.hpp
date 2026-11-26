#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "3d/physics/jolt/Layers.hpp"
#include "haylen/3d/physics/CollisionFilter.hpp"

namespace haylen::physics3d::jolt {

// The filter of a query of a world, which finds the bodies whose category shares a bit with the mask of the query, as `CollisionFilter::canFind` says.
class QueryFilter final : public JPH::ObjectLayerFilter {
  public:
    QueryFilter(const Layers& bodyLayers, const CollisionFilter& queryFilter) noexcept : layers(bodyLayers), filter(queryFilter) {}

    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override;

  private:
    const Layers& layers;
    CollisionFilter filter;
};

} // namespace haylen::physics3d::jolt
