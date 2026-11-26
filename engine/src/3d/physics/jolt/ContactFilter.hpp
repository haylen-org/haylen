#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ContactListener.h>

#include <cstdint>
#include <vector>

namespace haylen::physics3d::jolt {

// Rejects the contacts of the pairs of bodies that a joint holds without letting them collide, such as the parts of a ragdoll at their joints. Jolt asks it on the threads of the step, and the pairs change only between steps, on the thread that steps the world, in a sorted list that a step reads without a lock.
class ContactFilter final : public JPH::ContactListener {
  public:
    // A pair may be listed once for every joint that holds it.
    void addPair(std::uint32_t first, std::uint32_t second);
    void removePair(std::uint32_t first, std::uint32_t second);

    [[nodiscard]] JPH::ValidateResult OnContactValidate(const JPH::Body& first, const JPH::Body& second, JPH::RVec3Arg baseOffset, const JPH::CollideShapeResult& result) override;

  private:
    [[nodiscard]] static std::uint64_t keyOf(std::uint32_t first, std::uint32_t second) noexcept;

    std::vector<std::uint64_t> pairs;
};

} // namespace haylen::physics3d::jolt
