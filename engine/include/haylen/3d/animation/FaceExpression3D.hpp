#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"

namespace haylen::animation3d {

// The 52 face blend shapes that the face tracking of phones and headsets reports, by the names that this tracking gives them and that artists give the morph targets of faces, so the weights of a tracked face drive the targets of a model of the same names.
enum class FaceExpression3D : std::uint8_t {
    EyeBlinkLeft,
    EyeLookDownLeft,
    EyeLookInLeft,
    EyeLookOutLeft,
    EyeLookUpLeft,
    EyeSquintLeft,
    EyeWideLeft,
    EyeBlinkRight,
    EyeLookDownRight,
    EyeLookInRight,
    EyeLookOutRight,
    EyeLookUpRight,
    EyeSquintRight,
    EyeWideRight,
    JawForward,
    JawLeft,
    JawRight,
    JawOpen,
    MouthClose,
    MouthFunnel,
    MouthPucker,
    MouthLeft,
    MouthRight,
    MouthSmileLeft,
    MouthSmileRight,
    MouthFrownLeft,
    MouthFrownRight,
    MouthDimpleLeft,
    MouthDimpleRight,
    MouthStretchLeft,
    MouthStretchRight,
    MouthRollLower,
    MouthRollUpper,
    MouthShrugLower,
    MouthShrugUpper,
    MouthPressLeft,
    MouthPressRight,
    MouthLowerDownLeft,
    MouthLowerDownRight,
    MouthUpperUpLeft,
    MouthUpperUpRight,
    BrowDownLeft,
    BrowDownRight,
    BrowInnerUp,
    BrowOuterUpLeft,
    BrowOuterUpRight,
    CheekPuff,
    CheekSquintLeft,
    CheekSquintRight,
    NoseSneerLeft,
    NoseSneerRight,
    TongueOut,
};

} // namespace haylen::animation3d

namespace haylen::core {

template <> struct EnumNames<animation3d::FaceExpression3D> {
    using Value = animation3d::FaceExpression3D;

    static constexpr std::array<std::pair<std::string_view, Value>, 52> kNames{{
        {"eyeBlinkLeft", Value::EyeBlinkLeft}, {"eyeLookDownLeft", Value::EyeLookDownLeft}, {"eyeLookInLeft", Value::EyeLookInLeft}, {"eyeLookOutLeft", Value::EyeLookOutLeft}, {"eyeLookUpLeft", Value::EyeLookUpLeft}, {"eyeSquintLeft", Value::EyeSquintLeft}, {"eyeWideLeft", Value::EyeWideLeft}, {"eyeBlinkRight", Value::EyeBlinkRight}, {"eyeLookDownRight", Value::EyeLookDownRight}, {"eyeLookInRight", Value::EyeLookInRight}, {"eyeLookOutRight", Value::EyeLookOutRight}, {"eyeLookUpRight", Value::EyeLookUpRight}, {"eyeSquintRight", Value::EyeSquintRight}, {"eyeWideRight", Value::EyeWideRight}, {"jawForward", Value::JawForward}, {"jawLeft", Value::JawLeft}, {"jawRight", Value::JawRight}, {"jawOpen", Value::JawOpen}, {"mouthClose", Value::MouthClose}, {"mouthFunnel", Value::MouthFunnel}, {"mouthPucker", Value::MouthPucker}, {"mouthLeft", Value::MouthLeft}, {"mouthRight", Value::MouthRight}, {"mouthSmileLeft", Value::MouthSmileLeft}, {"mouthSmileRight", Value::MouthSmileRight}, {"mouthFrownLeft", Value::MouthFrownLeft}, {"mouthFrownRight", Value::MouthFrownRight}, {"mouthDimpleLeft", Value::MouthDimpleLeft}, {"mouthDimpleRight", Value::MouthDimpleRight}, {"mouthStretchLeft", Value::MouthStretchLeft}, {"mouthStretchRight", Value::MouthStretchRight}, {"mouthRollLower", Value::MouthRollLower}, {"mouthRollUpper", Value::MouthRollUpper}, {"mouthShrugLower", Value::MouthShrugLower}, {"mouthShrugUpper", Value::MouthShrugUpper}, {"mouthPressLeft", Value::MouthPressLeft}, {"mouthPressRight", Value::MouthPressRight}, {"mouthLowerDownLeft", Value::MouthLowerDownLeft}, {"mouthLowerDownRight", Value::MouthLowerDownRight}, {"mouthUpperUpLeft", Value::MouthUpperUpLeft}, {"mouthUpperUpRight", Value::MouthUpperUpRight}, {"browDownLeft", Value::BrowDownLeft}, {"browDownRight", Value::BrowDownRight}, {"browInnerUp", Value::BrowInnerUp}, {"browOuterUpLeft", Value::BrowOuterUpLeft}, {"browOuterUpRight", Value::BrowOuterUpRight}, {"cheekPuff", Value::CheekPuff}, {"cheekSquintLeft", Value::CheekSquintLeft}, {"cheekSquintRight", Value::CheekSquintRight}, {"noseSneerLeft", Value::NoseSneerLeft}, {"noseSneerRight", Value::NoseSneerRight}, {"tongueOut", Value::TongueOut},
    }};

    static std::optional<Value> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Value>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Value value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Value>::second)->first;
    }
};

} // namespace haylen::core
