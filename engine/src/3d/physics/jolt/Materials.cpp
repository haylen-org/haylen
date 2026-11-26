#include "3d/physics/jolt/Materials.hpp"

#include <algorithm>

namespace haylen::physics3d::jolt {

void Materials::install(JPH::PhysicsSystem& system) {
    system.SetCombineFriction(&combineFriction);
    system.SetCombineRestitution(&combineRestitution);
}

JPH::uint64 Materials::shiftOf(Body::Material which) noexcept {
    return which == Body::Material::Friction ? 0 : kRuleBits;
}

Body::CombineRule Materials::getRule(const JPH::Body& body, Body::Material which) noexcept {
    return static_cast<Body::CombineRule>((body.GetUserData() >> shiftOf(which)) & kRuleMask);
}

void Materials::setRule(JPH::Body& body, Body::Material which, Body::CombineRule value) noexcept {
    const JPH::uint64 shift = shiftOf(which);
    const JPH::uint64 cleared = body.GetUserData() & ~(kRuleMask << shift);
    body.SetUserData(cleared | (static_cast<JPH::uint64>(value) << shift));
}

float Materials::combine(Body::CombineRule rule, float first, float second) noexcept {
    switch (rule) {
    case Body::CombineRule::Minimum:
        return std::min(first, second);
    case Body::CombineRule::Multiply:
        return first * second;
    case Body::CombineRule::Maximum:
        return std::max(first, second);
    case Body::CombineRule::Average:
        break;
    }

    return (first + second) * 0.5F;
}

float Materials::combineFriction(const JPH::Body& first, const JPH::SubShapeID&, const JPH::Body& second, const JPH::SubShapeID&) {
    const Body::CombineRule rule = std::max(getRule(first, Body::Material::Friction), getRule(second, Body::Material::Friction));
    return combine(rule, first.GetFriction(), second.GetFriction());
}

float Materials::combineRestitution(const JPH::Body& first, const JPH::SubShapeID&, const JPH::Body& second, const JPH::SubShapeID&) {
    const Body::CombineRule rule = std::max(getRule(first, Body::Material::Restitution), getRule(second, Body::Material::Restitution));
    return combine(rule, first.GetRestitution(), second.GetRestitution());
}

} // namespace haylen::physics3d::jolt
