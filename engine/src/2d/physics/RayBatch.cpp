#include "haylen/2d/physics/RayBatch.hpp"

#include <stdexcept>
#include <string>

namespace haylen::physics2d {

void RayBatch::requireIndex(std::size_t index) const {
    if (index >= rays.size()) {
        throw std::out_of_range("Ray " + std::to_string(index) + " is outside the batch of " + std::to_string(rays.size()) + " rays.");
    }
}

void RayBatch::resize(std::size_t count) {
    rays.resize(count);
    results.resize(count);
}

void RayBatch::setRay(std::size_t index, math::Vec2 from, math::Vec2 to) {
    requireIndex(index);
    rays[index] = {from, to};
}

const math::Segment& RayBatch::getRay(std::size_t index) const {
    requireIndex(index);
    return rays[index];
}

const std::optional<RaycastHit>& RayBatch::getResult(std::size_t index) const {
    requireIndex(index);
    return results[index];
}

} // namespace haylen::physics2d
