#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "haylen/3d/physics/Body.hpp"

namespace haylen::physics3d::jolt {

// Combines the friction and the restitution of two touching bodies by the rules of the bodies, of which the one that comes later in `Body::CombineRule` wins. Jolt calls the combine functions on the threads of the step with the bodies alone, so the rules of a body live in the low bits of its user data, which nothing else of the backend uses.
class Materials final {
  public:
    // Makes the physics system combine by the rules of its bodies.
    static void install(JPH::PhysicsSystem& system);

    [[nodiscard]] static Body::CombineRule getRule(const JPH::Body& body, Body::Material which) noexcept;
    static void setRule(JPH::Body& body, Body::Material which, Body::CombineRule value) noexcept;

  private:
    static constexpr JPH::uint64 kRuleBits = 2;
    static constexpr JPH::uint64 kRuleMask = (JPH::uint64{1} << kRuleBits) - 1;

    [[nodiscard]] static JPH::uint64 shiftOf(Body::Material which) noexcept;
    [[nodiscard]] static float combine(Body::CombineRule rule, float first, float second) noexcept;
    [[nodiscard]] static float combineFriction(const JPH::Body& first, const JPH::SubShapeID& firstPart, const JPH::Body& second, const JPH::SubShapeID& secondPart);
    [[nodiscard]] static float combineRestitution(const JPH::Body& first, const JPH::SubShapeID& firstPart, const JPH::Body& second, const JPH::SubShapeID& secondPart);
};

} // namespace haylen::physics3d::jolt
