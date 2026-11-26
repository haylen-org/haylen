#include "3d/physics/jolt/QueryFilter.hpp"

namespace haylen::physics3d::jolt {

bool QueryFilter::ShouldCollide(JPH::ObjectLayer layer) const {
    return filter.canFind(layers.getFilter(layer));
}

} // namespace haylen::physics3d::jolt
