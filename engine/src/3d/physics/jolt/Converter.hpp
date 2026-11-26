#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Geometry/AABox.h>

#include "haylen/math/Box.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d::jolt {

// Converts the vectors, rotations and boxes of the engine to those of Jolt and back. Both measure in meters with `y` up, so a conversion only copies.
class Converter final {
  public:
    [[nodiscard]] static JPH::Vec3 toJolt(math::Vec3 value) noexcept {
        return {value.x, value.y, value.z};
    }
    [[nodiscard]] static JPH::Quat toJolt(const math::Quat& value) noexcept {
        return {value.x, value.y, value.z, value.w};
    }
    [[nodiscard]] static math::Vec3 fromJolt(JPH::Vec3Arg value) noexcept {
        return {value.GetX(), value.GetY(), value.GetZ()};
    }
    [[nodiscard]] static math::Quat fromJolt(JPH::QuatArg value) noexcept {
        return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
    }
    [[nodiscard]] static math::Box fromJolt(const JPH::AABox& value) noexcept {
        return {fromJolt(value.mMin), fromJolt(value.mMax)};
    }
};

} // namespace haylen::physics3d::jolt
