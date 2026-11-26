#pragma once

#include <optional>
#include <string_view>

#include "haylen/3d/physics/Joint.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

// Checks every value that a joint takes, when it is made and when it changes, so each backend receives only values it can hold. Each check throws `std::invalid_argument` with a sentence that names the value and says what it takes.
class JointValidator final {
  public:
    // Checks the options of a joint of the type, whose second body is `hasSecond` and whose bodies of `joints` the world checked.
    static void checkOptions(Joint::Type type, const Joint::Options& options);

    static void requireBreakLimit(std::optional<float> value);
    static void requireLimits(Joint::Type type, float lower, float upper);
    static void requireSpring(const Joint::Spring& value);
    static void requireNonNegative(std::string_view what, float value);
    static void requireConeAngle(float value);
    static void requireRatio(float value);
    static void requireMotor(const Joint::Motor& value);
    static void requireRotation(std::string_view what, const math::Quat& value);
    static void requireFinite(std::string_view what, math::Vec3 value);
    static void requireAxis(Joint::Axis axis, const Joint::AxisSettings& value);

    // Throws `std::logic_error` when the joint of the type has no motor in the slot.
    static void requireMotorSlot(Joint::Type type, Joint::MotorSlot slot);

  private:
    // A rotation may be off a length of one, and an axis off a right angle to its normal, by this much.
    static constexpr float kTolerance = 1.0e-3F;

    // The most solver passes that a joint asks for.
    static constexpr int kMaxSteps = 255;

    static void requireDirection(std::string_view what, math::Vec3 value);
    static void requirePerpendicular(math::Vec3 axis, math::Vec3 normal);
    [[nodiscard]] static bool turnsAroundAxis(Joint::Type type) noexcept;
};

} // namespace haylen::physics3d
