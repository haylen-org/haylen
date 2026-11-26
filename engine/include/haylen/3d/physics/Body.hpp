#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/3d/physics/CollisionFilter.hpp"
#include "haylen/3d/physics/Shape.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::physics3d {

class World;

// Handle to a rigid body of a 3D world, in meters, kilograms and radians. Handles stay cheap to copy, report `false` from `isValid` once the body or its world is destroyed, even after a new world or body takes its place, and throw `std::logic_error` when used after that.
class Body final {
  public:
    enum class Type : std::uint8_t {
        Static,
        Kinematic,
        Dynamic,
    };

    // A discrete body moves in jumps of one step, and a body of linear cast sweeps its shape from where it was to where it goes, so a fast body never passes a thin wall.
    enum class MotionQuality : std::uint8_t {
        Discrete,
        LinearCast,
    };

    // How the friction or restitution of two touching bodies combine. Of the rules of the two bodies, the one that comes later in this list wins.
    enum class CombineRule : std::uint8_t {
        Average,
        Minimum,
        Multiply,
        Maximum,
    };

    // A body locked along an axis of the world keeps its place on that axis, and a body locked around an axis keeps its rotation around it, whatever pushes it, while it moves freely along the others. A dynamic body keeps at least one axis free.
    struct MotionLocks {
        bool x = false;
        bool y = false;
        bool z = false;
        bool rotationX = false;
        bool rotationY = false;
        bool rotationZ = false;

        [[nodiscard]] friend bool operator==(const MotionLocks&, const MotionLocks&) noexcept = default;
    };

    struct Transform {
        math::Vec3 position{};
        math::Quat rotation = math::Quat::identity();
    };

    // A body takes the mass and inertia of its shape and density unless the options give them. The inertia is the diagonal of the inertia around the center of mass along the axes of the body, in kilograms times square meters, and setting the mass alone scales the inertia of the shape to it. The center of mass moves the point the body turns around, away from where its shape puts it. A kinematic body or a sensor that collides with static bodies also meets static and kinematic bodies, such as a trigger that finds the walls. Unset internal edge removal takes the setting of the world. Speeds are in meters per second and radians per second.
    struct Options {
        Type type = Type::Dynamic;
        std::shared_ptr<const Shape> shape{};
        math::Vec3 position{};
        math::Quat rotation = math::Quat::identity();
        math::Vec3 linearVelocity{};
        math::Vec3 angularVelocity{};
        MotionQuality motionQuality = MotionQuality::Discrete;
        MotionLocks locks{};
        std::optional<float> mass{};
        std::optional<math::Vec3> inertia{};
        std::optional<math::Vec3> centerOfMass{};
        float linearDamping = 0.05F;
        float angularDamping = 0.05F;
        float gravityScale = 1.0F;
        float maxLinearSpeed = 500.0F;
        float maxAngularSpeed = 47.1238898F;
        float friction = 0.2F;
        float restitution = 0.0F;
        CombineRule frictionCombine = CombineRule::Average;
        CombineRule restitutionCombine = CombineRule::Average;
        bool sleepEnabled = true;
        bool sensor = false;
        bool collideWithStatic = false;
        std::optional<bool> enhancedInternalEdgeRemoval{};
        bool enabled = true;
        CollisionFilter filter{};

        // Throws `std::invalid_argument` with a sentence that names the option for a value that is not finite or out of its range, a shape of another backend than `backend` and a shape or locks that do not suit the type.
        void validate(std::string_view backend) const;
    };

    // The vectors of a body that `Simulation` reads and writes. The world center is where the center of mass is in the world, and only reads.
    enum class Vector : std::uint8_t {
        LinearVelocity,
        AngularVelocity,
        CenterOfMass,
        Inertia,
        WorldCenter,
    };

    // The numbers of a body that `Simulation` reads and writes.
    enum class Value : std::uint8_t {
        Mass,
        LinearDamping,
        AngularDamping,
        GravityScale,
        MaxLinearSpeed,
        MaxAngularSpeed,
        Friction,
        Restitution,
    };

    // The switches of a body that `Simulation` reads and writes.
    enum class Flag : std::uint8_t {
        SleepEnabled,
        Awake,
        Sensor,
        CollideWithStatic,
        EnhancedInternalEdgeRemoval,
        Enabled,
    };

    // What a push gives a body: a force and a torque act over the next step, and an impulse and an angular impulse change its velocities at once.
    enum class Push : std::uint8_t {
        Force,
        Impulse,
        Torque,
        AngularImpulse,
    };

    enum class Material : std::uint8_t {
        Friction,
        Restitution,
    };

    Body() = default;
    // A handle names its world by the key of its slot and generation, which `World` hands out, and its body by the id of the simulation.
    Body(World* owner, std::uint64_t worldKey, std::uint32_t bodyId) noexcept : world(owner), key(worldKey), id(bodyId) {}

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
    [[nodiscard]] static std::optional<MotionQuality> motionQualityFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view motionQualityName(MotionQuality value) noexcept;
    [[nodiscard]] static std::optional<CombineRule> combineRuleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view combineRuleName(CombineRule value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;

    // Destroys the body with the joints that hold it. Destroying a destroyed body does nothing.
    void destroy();

    // A static body never moves, a kinematic body moves only as its velocity or `moveTo` says, and a dynamic body moves by every force and contact. Throws `std::invalid_argument` when the shape does not suit the type, as `World::createBody` says.
    [[nodiscard]] Type getType() const;
    void setType(Type value);
    [[nodiscard]] MotionQuality getMotionQuality() const;
    void setMotionQuality(MotionQuality value);
    [[nodiscard]] MotionLocks getMotionLocks() const;
    void setMotionLocks(const MotionLocks& value);

    [[nodiscard]] math::Vec3 getPosition() const;
    [[nodiscard]] math::Quat getRotation() const;
    [[nodiscard]] Transform getTransform() const;

    // Teleports the body, which suits spawning and respawning. A teleport skips every sweep and pushes out what the body lands in, so bodies that should push and carry others move with `moveTo` instead.
    void setPosition(math::Vec3 value);
    void setRotation(const math::Quat& value);
    void setTransform(const Transform& value);

    // Gives a kinematic or dynamic body the velocities that bring it to the transform after `seconds`, normally one fixed step, so a kinematic platform driven by a path or a tween pushes and carries what touches it. Throws `std::invalid_argument` unless the time is positive, and `std::logic_error` for a static body.
    void moveTo(const Transform& target, float seconds);

    // Throws `std::invalid_argument` for a velocity faster than the max speed of the body, and `std::logic_error` for a static body.
    [[nodiscard]] math::Vec3 getLinearVelocity() const;
    void setLinearVelocity(math::Vec3 value);
    [[nodiscard]] math::Vec3 getAngularVelocity() const;
    void setAngularVelocity(math::Vec3 value);

    // Returns the velocity of a world point that moves with the body, with the spin of the body, such as the speed of the ground under the feet of a character.
    [[nodiscard]] math::Vec3 getVelocityAt(math::Vec3 point) const;

    // A force or impulse at a world point also turns the body, and one without a point acts at the center of mass. Forces and torques act over the next step. Throws `std::logic_error` unless the body is dynamic.
    void applyForce(math::Vec3 force, std::optional<math::Vec3> point = std::nullopt);
    void applyImpulse(math::Vec3 impulse, std::optional<math::Vec3> point = std::nullopt);
    void applyTorque(math::Vec3 torque);
    void applyAngularImpulse(math::Vec3 impulse);

    // The mass in kilograms, the inertia around the center of mass along the axes of the body and the center of mass in the space of the body. Setting the mass alone scales the inertia with it, and `resetMass` takes every one from the shape and its density again. Throws `std::invalid_argument` for a mass or inertia that is not positive and finite.
    [[nodiscard]] float getMass() const;
    void setMass(float value);
    [[nodiscard]] math::Vec3 getInertia() const;
    void setInertia(math::Vec3 value);
    [[nodiscard]] math::Vec3 getCenterOfMass() const;
    void setCenterOfMass(math::Vec3 value);
    void resetMass();
    [[nodiscard]] math::Vec3 getWorldCenter() const;

    // Throws `std::invalid_argument` for a value that is negative or not finite.
    [[nodiscard]] float getLinearDamping() const;
    void setLinearDamping(float value);
    [[nodiscard]] float getAngularDamping() const;
    void setAngularDamping(float value);
    [[nodiscard]] float getMaxLinearSpeed() const;
    void setMaxLinearSpeed(float value);
    [[nodiscard]] float getMaxAngularSpeed() const;
    void setMaxAngularSpeed(float value);
    [[nodiscard]] float getFriction() const;
    void setFriction(float value);
    [[nodiscard]] float getRestitution() const;
    void setRestitution(float value);

    // Throws `std::invalid_argument` for a scale that is not finite.
    [[nodiscard]] float getGravityScale() const;
    void setGravityScale(float value);

    [[nodiscard]] CombineRule getFrictionCombine() const;
    void setFrictionCombine(CombineRule value);
    [[nodiscard]] CombineRule getRestitutionCombine() const;
    void setRestitutionCombine(CombineRule value);

    [[nodiscard]] bool isSleepEnabled() const;
    void setSleepEnabled(bool value);
    [[nodiscard]] bool isAwake() const;
    void setAwake(bool value);
    [[nodiscard]] bool isSensor() const;
    void setSensor(bool value);
    [[nodiscard]] bool isCollidingWithStatic() const;
    void setCollideWithStatic(bool value);
    [[nodiscard]] bool hasEnhancedInternalEdgeRemoval() const;
    void setEnhancedInternalEdgeRemoval(bool value);

    // A body that is not enabled leaves the simulation with its state, so nothing touches it and it moves nowhere until it is enabled again.
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool value);

    // The filter belongs to the whole body, so every part of its shape collides the same way.
    [[nodiscard]] CollisionFilter getFilter() const;
    void setFilter(const CollisionFilter& value);

    // Replaces the shape and takes the mass of the new shape unless the mass was set. Throws `std::invalid_argument` as `World::createBody` does for the shape.
    [[nodiscard]] std::shared_ptr<const Shape> getShape() const;
    void setShape(std::shared_ptr<const Shape> value);

    // Returns the box that holds the body in the world.
    [[nodiscard]] math::Box getBounds() const;

    // The children of the own copy of a mutable compound that the body took, in the space of the body. Throws `std::logic_error` when the shape of the body is not a mutable compound and `std::out_of_range` for an index past the last child.
    [[nodiscard]] std::size_t getChildCount() const;
    std::size_t addChild(const Shape::Child& child);
    void removeChild(std::size_t index);
    void setChildTransform(std::size_t index, const Transform& value);

    [[nodiscard]] std::uint32_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] std::uint64_t getWorldKey() const noexcept {
        return key;
    }
    [[nodiscard]] World* getWorld() const noexcept {
        return world;
    }
    [[nodiscard]] friend bool operator==(const Body& lhs, const Body& rhs) noexcept {
        return lhs.key == rhs.key && lhs.id == rhs.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 3> kTypeNames;
    static const std::array<std::pair<std::string_view, MotionQuality>, 2> kMotionQualityNames;
    static const std::array<std::pair<std::string_view, CombineRule>, 4> kCombineRuleNames;

    // A rotation may be off a length of one by this much.
    static constexpr float kRotationTolerance = 1.0e-3F;

    // Returns the world of a live body. Throws `std::logic_error` once the world or the body is destroyed.
    [[nodiscard]] World& checkedWorld() const;
    void requireDynamic(std::string_view action) const;
    void requireMutableCompound() const;

    static void requireNonNegative(std::string_view what, float value);
    static void requireFinite(std::string_view what, float value);
    static void requireFinite(std::string_view what, math::Vec3 value);
    static void requirePositive(std::string_view what, math::Vec3 value);
    static void requireRotation(std::string_view what, const math::Quat& value);
    static void requireSpeed(math::Vec3 velocity, float limit, std::string_view what, std::string_view option);
    static void requireTransform(const Transform& value);
    static void requireShape(const std::shared_ptr<const Shape>& shape, Type type, std::string_view backend);
    static void requireFreeAxis(const MotionLocks& locks, Type type);

    World* world = nullptr;
    std::uint64_t key = 0;
    std::uint32_t id = 0;
};

} // namespace haylen::physics3d
