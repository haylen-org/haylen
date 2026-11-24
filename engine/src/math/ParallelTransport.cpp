#include "math/ParallelTransport.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "haylen/math/Quat.hpp"

namespace haylen::math {

void ParallelTransport::carry(std::span<CurveFrame> frames, Vec3 up, bool closed) {
    const Vec3 lift = up.getNormalized();
    if (frames.empty()) {
        return;
    }

    // The first normal is the part of the up that crosses the first tangent, or of the axis of the world most across it.
    const Vec3 tangent = frames[0].tangent;
    Vec3 across = lift;
    if (Vec3::cross(lift, tangent).getLength() < 1e-4F) {
        const std::array<Vec3, 3> axes{Vec3::right(), Vec3::up(), Vec3{0.0F, 0.0F, 1.0F}};
        across = *std::ranges::min_element(axes, {}, [tangent](Vec3 axis) { return std::fabs(Vec3::dot(axis, tangent)); });
    }

    frames[0].normal = (across - tangent * Vec3::dot(across, tangent)).getNormalized();
    for (std::size_t index = 0; index + 1 < frames.size(); ++index) {
        const CurveFrame& current = frames[index];
        CurveFrame& next = frames[index + 1];
        const Vec3 chord = next.position - current.position;
        const float chordSquared = chord.getLengthSquared();
        if (chordSquared == 0.0F) {
            next.normal = current.normal;
            continue;
        }

        const Vec3 reflectedNormal = current.normal - chord * (2.0F * Vec3::dot(chord, current.normal) / chordSquared);
        const Vec3 reflectedTangent = current.tangent - chord * (2.0F * Vec3::dot(chord, current.tangent) / chordSquared);
        const Vec3 turn = next.tangent - reflectedTangent;
        const float turnSquared = turn.getLengthSquared();
        const Vec3 carried = turnSquared == 0.0F ? reflectedNormal : reflectedNormal - turn * (2.0F * Vec3::dot(turn, reflectedNormal) / turnSquared);
        next.normal = (carried - next.tangent * Vec3::dot(carried, next.tangent)).getNormalized();
    }

    // A closed curve turns its frames a little more at every step, so the twist the loop gathers is gone when it returns to the first frame.
    if (closed && frames.size() > 1) {
        const Vec3 returned = frames.back().normal;
        const float twist = std::atan2(Vec3::dot(Vec3::cross(returned, frames[0].normal), frames[0].tangent), Vec3::dot(returned, frames[0].normal));
        for (std::size_t index = 1; index < frames.size(); ++index) {
            const float share = twist * static_cast<float>(index) / static_cast<float>(frames.size() - 1);
            frames[index].normal = Quat::fromAxisAngle(frames[index].tangent, share).rotate(frames[index].normal);
        }
    }

    for (CurveFrame& frame : frames) {
        frame.binormal = Vec3::cross(frame.tangent, frame.normal);
    }
}

} // namespace haylen::math
