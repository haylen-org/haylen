#include "haylen/2d/physics/Ragdoll.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"

namespace haylen::physics2d {

const std::array<std::string_view, Ragdoll::kPartCount> Ragdoll::kNames{"head", "chest", "hips", "upperArmLeft", "lowerArmLeft", "upperArmRight", "lowerArmRight", "upperLegLeft", "lowerLegLeft", "upperLegRight", "lowerLegRight"};

// The head is a capsule with both ends at its center, which makes it a circle.
const std::array<Ragdoll::Bone, Ragdoll::kPartCount> Ragdoll::kBones{{
    {-0.42F, -0.42F, 0.075F},
    {-0.31F, -0.12F, 0.07F},
    {-0.06F, 0.0F, 0.065F},
    {-0.29F, -0.13F, 0.035F},
    {-0.11F, 0.04F, 0.03F},
    {-0.29F, -0.13F, 0.035F},
    {-0.11F, 0.04F, 0.03F},
    {0.04F, 0.24F, 0.045F},
    {0.27F, 0.46F, 0.04F},
    {0.04F, 0.24F, 0.045F},
    {0.27F, 0.46F, 0.04F},
}};

// Facing right with y pointing down, positive angles turn a limb backward: knees bend back while elbows and hips swing forward.
const std::array<Ragdoll::Link, Ragdoll::kPartCount - 1> Ragdoll::kLinks{{
    {Part::Chest, Part::Head, -0.35F, -0.6F, 0.6F},
    {Part::Chest, Part::Hips, -0.09F, -0.5F, 0.5F},
    {Part::Chest, Part::UpperArmLeft, -0.3F, -2.8F, 0.8F},
    {Part::UpperArmLeft, Part::LowerArmLeft, -0.12F, -2.4F, 0.0F},
    {Part::Chest, Part::UpperArmRight, -0.3F, -2.8F, 0.8F},
    {Part::UpperArmRight, Part::LowerArmRight, -0.12F, -2.4F, 0.0F},
    {Part::Hips, Part::UpperLegLeft, 0.02F, -1.6F, 0.5F},
    {Part::UpperLegLeft, Part::LowerLegLeft, 0.255F, 0.0F, 2.4F},
    {Part::Hips, Part::UpperLegRight, 0.02F, -1.6F, 0.5F},
    {Part::UpperLegRight, Part::LowerLegRight, 0.255F, 0.0F, 2.4F},
}};

std::optional<Ragdoll::Part> Ragdoll::partFromName(std::string_view name) noexcept {
    const auto found = std::find(kNames.begin(), kNames.end(), name);
    if (found == kNames.end()) {
        return std::nullopt;
    }
    return static_cast<Part>(found - kNames.begin());
}

std::string_view Ragdoll::partName(Part value) noexcept {
    return kNames[static_cast<std::size_t>(value)];
}

Ragdoll Ragdoll::create(World& world, const Options& options) {
    if (options.height <= 0.0F || options.group >= 0) {
        throw std::invalid_argument("A ragdoll needs a positive height and a negative collision group.");
    }

    // A ragdoll that fails halfway, such as on a bad material, leaves no bodies behind.
    Ragdoll ragdoll;
    try {
        ragdoll.build(world, options);
    } catch (...) {
        ragdoll.destroy();
        throw;
    }
    return ragdoll;
}

void Ragdoll::build(World& world, const Options& options) {
    const float scale = options.height;
    const Shape::Options shape{.density = options.density, .friction = options.friction, .filter = {.group = options.group}};
    for (std::size_t part = 0; part < kPartCount; ++part) {
        const Bone& bone = kBones[part];
        const float middle = (bone.top + bone.bottom) * 0.5F;
        const float reach = (bone.bottom - bone.top) * 0.5F * scale;
        Body& body = bodies[part];
        body = world.createBody({.position = options.position + math::Vec2{0.0F, middle * scale}, .velocity = options.velocity});
        if (reach > 0.0F) {
            body.addCapsule({0.0F, -reach}, {0.0F, reach}, bone.radius * scale, shape);
        } else {
            body.addCircle(bone.radius * scale, shape);
        }
    }

    for (const Link& link : kLinks) {
        Joint::Options joint{.anchorA = options.position + math::Vec2{0.0F, link.at * scale}, .enableLimit = true, .lower = link.lower, .upper = link.upper};
        if (options.jointFriction > 0.0F) {
            joint.enableMotor = true;
            joint.maxMotorTorque = options.jointFriction;
        }
        joints.push_back(world.createJoint(Joint::Type::Revolute, getBody(link.parent), getBody(link.child), joint));
    }
}

bool Ragdoll::isValid() const noexcept {
    return std::all_of(bodies.begin(), bodies.end(), [](const Body& body) { return body.isValid(); });
}

void Ragdoll::destroy() {
    for (Body& body : bodies) {
        body.destroy();
    }
    joints.clear();
}

} // namespace haylen::physics2d
