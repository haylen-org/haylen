#include "3d/physics/jolt/ContactFilter.hpp"

#include <Jolt/Physics/Body/Body.h>

#include <algorithm>
#include <utility>

namespace haylen::physics3d::jolt {

std::uint64_t ContactFilter::keyOf(std::uint32_t first, std::uint32_t second) noexcept {
    const auto [low, high] = std::minmax(first, second);
    return (static_cast<std::uint64_t>(high) << 32) | low;
}

void ContactFilter::addPair(std::uint32_t first, std::uint32_t second) {
    const std::uint64_t key = keyOf(first, second);
    pairs.insert(std::ranges::upper_bound(pairs, key), key);
}

void ContactFilter::removePair(std::uint32_t first, std::uint32_t second) {
    const auto found = std::ranges::lower_bound(pairs, keyOf(first, second));
    if (found != pairs.end() && *found == keyOf(first, second)) {
        pairs.erase(found);
    }
}

JPH::ValidateResult ContactFilter::OnContactValidate(const JPH::Body& first, const JPH::Body& second, JPH::RVec3Arg, const JPH::CollideShapeResult&) {
    const bool joined = std::ranges::binary_search(pairs, keyOf(first.GetID().GetIndexAndSequenceNumber(), second.GetID().GetIndexAndSequenceNumber()));
    return joined ? JPH::ValidateResult::RejectAllContactsForThisBodyPair : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

} // namespace haylen::physics3d::jolt
