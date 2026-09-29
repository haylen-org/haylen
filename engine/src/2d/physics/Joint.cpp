#include "haylen/2d/physics/Joint.hpp"

#include <box2d/box2d.h>

#include <stdexcept>

#include "2d/physics/Box2DConverter.hpp"
#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

const std::array<std::pair<std::string_view, Joint::Type>, 8> Joint::kTypeNames{{{"distance", Type::Distance}, {"revolute", Type::Revolute}, {"prismatic", Type::Prismatic}, {"weld", Type::Weld}, {"wheel", Type::Wheel}, {"mouse", Type::Mouse}, {"motor", Type::Motor}, {"filter", Type::Filter}}};

std::optional<Joint::Type> Joint::typeFromName(std::string_view name) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (text == name) {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view Joint::typeName(Type value) noexcept {
    for (const auto& [text, type] : kTypeNames) {
        if (type == value) {
            return text;
        }
    }
    return kTypeNames.front().first;
}

std::uint64_t Joint::checkedId() const {
    if (!isValid()) {
        throw std::logic_error("The physics joint was destroyed.");
    }
    return id;
}

bool Joint::isValid() const noexcept {
    return world != nullptr && b2Joint_IsValid(b2LoadJointId(id));
}

void Joint::destroy() {
    if (isValid()) {
        b2DestroyJoint(b2LoadJointId(id));
    }
}

std::uint64_t Joint::checkedMouseId() const {
    const std::uint64_t checked = checkedId();
    if (b2Joint_GetType(b2LoadJointId(checked)) != b2_mouseJoint) {
        throw std::logic_error("Only mouse joints have a target.");
    }
    return checked;
}

math::Vec2 Joint::getTarget() const {
    return Box2DConverter::toPixels(b2MouseJoint_GetTarget(b2LoadJointId(checkedMouseId())), world->getPixelsPerMeter());
}

void Joint::setTarget(math::Vec2 value) {
    b2MouseJoint_SetTarget(b2LoadJointId(checkedMouseId()), Box2DConverter::toMeters(value, world->getPixelsPerMeter()));
}

float Joint::getMotorSpeed() const {
    const b2JointId joint = b2LoadJointId(checkedId());
    switch (b2Joint_GetType(joint)) {
    case b2_revoluteJoint:
        return b2RevoluteJoint_GetMotorSpeed(joint);
    case b2_prismaticJoint:
        return b2PrismaticJoint_GetMotorSpeed(joint) * world->getPixelsPerMeter();
    case b2_wheelJoint:
        return b2WheelJoint_GetMotorSpeed(joint);
    default:
        throw std::logic_error("Only revolute, prismatic and wheel joints have a motor speed.");
    }
}

void Joint::setMotorSpeed(float value) {
    const b2JointId joint = b2LoadJointId(checkedId());
    switch (b2Joint_GetType(joint)) {
    case b2_revoluteJoint:
        b2RevoluteJoint_SetMotorSpeed(joint, value);
        return;
    case b2_prismaticJoint:
        b2PrismaticJoint_SetMotorSpeed(joint, value / world->getPixelsPerMeter());
        return;
    case b2_wheelJoint:
        b2WheelJoint_SetMotorSpeed(joint, value);
        return;
    default:
        throw std::logic_error("Only revolute, prismatic and wheel joints have a motor speed.");
    }
}

} // namespace haylen::physics2d
