#include "3d/physics/JointValidator.hpp"

#include <cmath>
#include <format>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::physics3d {

void JointValidator::requireNonNegative(std::string_view what, float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument(std::format("The {} of a 3D joint must be a finite number of zero or more.", what));
    }
}

void JointValidator::requireFinite(std::string_view what, math::Vec3 value) {
    if (!value.isFinite()) {
        throw std::invalid_argument(std::format("The {} of a 3D joint must be a vector of three finite numbers.", what));
    }
}

void JointValidator::requireDirection(std::string_view what, math::Vec3 value) {
    requireFinite(what, value);
    if (value.getLength() <= kTolerance) {
        throw std::invalid_argument(std::format("The {} of a 3D joint must point somewhere, so it cannot be zero.", what));
    }
}

void JointValidator::requirePerpendicular(math::Vec3 axis, math::Vec3 normal) {
    if (std::abs(math::Vec3::dot(axis.getNormalized(), normal.getNormalized())) > kTolerance) {
        throw std::invalid_argument("The normal of a 3D joint must stand at a right angle to its axis.");
    }
}

void JointValidator::requireRotation(std::string_view what, const math::Quat& value) {
    const float length = value.getLength();
    if (!std::isfinite(length) || std::abs(length - 1.0F) > kTolerance) {
        throw std::invalid_argument(std::format("The {} of a 3D joint must be a rotation, a quaternion of length one.", what));
    }
}

void JointValidator::requireBreakLimit(std::optional<float> value) {
    if (value && (!std::isfinite(*value) || *value < 0.0F)) {
        throw std::invalid_argument("The break force and the break torque of a 3D joint must be finite numbers of zero or more.");
    }
}

bool JointValidator::turnsAroundAxis(Joint::Type type) noexcept {
    return type == Joint::Type::Hinge || type == Joint::Type::SwingTwist;
}

void JointValidator::requireLimits(Joint::Type type, float lower, float upper) {
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper) {
        throw std::invalid_argument("The limits of a 3D joint must be finite numbers, the lower one not above the upper one.");
    }

    if (turnsAroundAxis(type) && (lower < -math::Math::kPi || upper > math::Math::kPi)) {
        throw std::invalid_argument("The limits of the angle of a 3D joint lie within half a turn, from -pi to pi radians.");
    }

    if ((type == Joint::Type::Distance || type == Joint::Type::Pulley) && lower < 0.0F) {
        throw std::invalid_argument("The limits of the length of a 3D joint must be zero or more.");
    }
}

void JointValidator::requireSpring(const Joint::Spring& value) {
    requireNonNegative("frequency of a spring", value.frequency);
    requireNonNegative("stiffness of a spring", value.stiffness);
    requireNonNegative("damping of a spring", value.damping);
}

void JointValidator::requireConeAngle(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > math::Math::kPi) {
        throw std::invalid_argument("The half angle of the cone of a 3D joint must be from 0 to pi radians.");
    }
}

void JointValidator::requireRatio(float value) {
    if (!std::isfinite(value) || value == 0.0F) {
        throw std::invalid_argument("The ratio of a 3D joint must be a finite number that is not zero.");
    }
}

void JointValidator::requireMotor(const Joint::Motor& value) {
    if (!std::isfinite(value.targetVelocity) || !std::isfinite(value.targetPosition)) {
        throw std::invalid_argument("The targets of the motor of a 3D joint must be finite numbers.");
    }

    if (std::isnan(value.maxForce) || value.maxForce < 0.0F || std::isnan(value.maxTorque) || value.maxTorque < 0.0F) {
        throw std::invalid_argument("The max force and the max torque of the motor of a 3D joint must be zero or more.");
    }

    requireSpring(value.spring);
}

void JointValidator::requireAxis(Joint::Axis axis, const Joint::AxisSettings& value) {
    const bool turns = axis == Joint::Axis::RotationX || axis == Joint::Axis::RotationY || axis == Joint::Axis::RotationZ;
    if (value.mode == Joint::AxisMode::Limited) {
        requireLimits(turns ? Joint::Type::Hinge : Joint::Type::Slider, value.lower, value.upper);
    }

    requireSpring(value.limitSpring);
    requireNonNegative("friction of an axis", value.friction);
    requireMotor(value.motor);
}

void JointValidator::requireMotorSlot(Joint::Type type, Joint::MotorSlot slot) {
    const bool main = slot == Joint::MotorSlot::Main && (type == Joint::Type::Hinge || type == Joint::Type::Slider || type == Joint::Type::Path || type == Joint::Type::SwingTwist);
    const bool swing = slot == Joint::MotorSlot::Swing && type == Joint::Type::SwingTwist;
    const bool axis = slot != Joint::MotorSlot::Main && slot != Joint::MotorSlot::Swing && type == Joint::Type::SixDof;
    if (!main && !swing && !axis) {
        throw std::logic_error(std::format("A 3D joint of the type \"{}\" has no such motor. Hinges, sliders, paths and swing twist joints have a main motor, swing twist joints a swing motor, and six degrees of freedom joints a motor for each axis.", Joint::typeName(type)));
    }
}

// Every type reads the points, its axes and its own settings, and every other setting keeps its default without effect.
void JointValidator::checkOptions(Joint::Type type, const Joint::Options& options) {
    requireFinite("point", options.point);
    if (options.secondPoint) {
        requireFinite("second point", *options.secondPoint);
    }

    if (type == Joint::Type::Hinge || type == Joint::Type::Slider || type == Joint::Type::Cone || type == Joint::Type::SwingTwist || type == Joint::Type::SixDof) {
        requireDirection("axis", options.axis);
        requireDirection("normal", options.normal);
        requirePerpendicular(options.axis, options.normal);
    }

    if (options.lower.has_value() != options.upper.has_value()) {
        throw std::invalid_argument("A 3D joint takes both of its limits or neither.");
    }

    if (options.lower) {
        requireLimits(type, *options.lower, *options.upper);
    }

    requireSpring(options.limitSpring);
    requireNonNegative("friction", options.friction);
    requireConeAngle(options.halfConeAngle);
    requireConeAngle(options.planeHalfConeAngle);
    requireMotor(options.motor);
    requireMotor(options.swingMotor);
    requireRotation("target rotation", options.targetRotation);
    requireFinite("target angular velocity", options.targetAngularVelocity);
    for (std::size_t axis = 0; axis < options.axes.size(); ++axis) {
        requireAxis(static_cast<Joint::Axis>(axis), options.axes[axis]);
    }

    if (type == Joint::Type::Path) {
        if (!options.path || options.path->getPoints().size() < 2) {
            throw std::invalid_argument("A path joint needs the option \"path\", a spline of two points or more.");
        }

        if (!std::isfinite(options.pathFraction) || options.pathFraction < 0.0F || options.pathFraction > 1.0F) {
            throw std::invalid_argument("The path fraction of a 3D joint must be from 0 to 1.");
        }
    }

    if (type == Joint::Type::Gear || type == Joint::Type::RackAndPinion) {
        requireDirection("axis", options.axis);
        requireDirection("second axis", options.secondAxis);
    }

    requireRatio(options.ratio);
    requireFinite("fixed point", options.fixedPoint);
    requireFinite("second fixed point", options.secondFixedPoint);
    if (options.priority < 0 || options.velocitySteps < 0 || options.velocitySteps > kMaxSteps || options.positionSteps < 0 || options.positionSteps > kMaxSteps) {
        throw std::invalid_argument("The priority of a 3D joint must be zero or more, and its velocity and position steps from 0 to 255.");
    }

    requireBreakLimit(options.breakForce);
    requireBreakLimit(options.breakTorque);
}

} // namespace haylen::physics3d
