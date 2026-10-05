#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Handle to a rigid body. Every length is in world units and every mass in kilograms, so forces and impulses scale with world units and torques, inertias and angular impulses with squared world units, and the world converts them all to meters for the solver.
class Body final {
  public:
    enum class Type : std::uint8_t {
        Static,
        Kinematic,
        Dynamic,
    };

    // A bullet sweeps against dynamic and kinematic bodies too, so a fast projectile never passes a thin moving wall. Fast rotation, which only creation sets, lifts the limit of an eighth of a turn per step that fast wheels pass. The sleep threshold is the speed in world units per second below which the body may fall asleep, 0.05 meters per second when unset.
    struct Options {
        Type type = Type::Dynamic;
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 velocity{};
        float angularVelocity = 0.0F;
        float linearDamping = 0.0F;
        float angularDamping = 0.0F;
        float gravityScale = 1.0F;
        bool fixedRotation = false;
        bool bullet = false;
        bool fastRotation = false;
        bool sleepEnabled = true;
        std::optional<float> sleepThreshold;
    };

    // The mass in kilograms, the center of mass in the space of the body and the rotational inertia around that center in kilograms times squared world units.
    struct MassData {
        float mass = 0.0F;
        math::Vec2 center{};
        float inertia = 0.0F;
    };

    // A contact of the body that touches now: its own shape, the other shape, the first contact point, the normal from the shape of the body toward the other one and the impulse that pushed them apart in the last step.
    struct Contact {
        Shape shape;
        Shape other;
        math::Vec2 point{};
        math::Vec2 normal{};
        float impulse = 0.0F;
    };

    Body() = default;
    // A handle remembers the generation of its world, so it turns invalid once that world is destroyed, even after a new world reuses its slot.
    Body(World* owner, std::uint64_t handle) noexcept;

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Type getType() const;
    void setType(Type value);
    [[nodiscard]] math::Vec2 getPosition() const;
    [[nodiscard]] float getRotation() const;

    // Teleports the body, which suits spawning and respawning. A teleport skips every sweep and shoves what the body lands in, so bodies that should push and carry others move with `moveTo` instead.
    void setTransform(math::Vec2 position, float rotation);

    // Gives the body the velocity that brings it to the transform after `seconds`, normally one fixed step, so a kinematic platform driven by a path or a tween pushes and carries what touches it. Throws `std::invalid_argument` unless the time is positive.
    void moveTo(math::Vec2 position, float rotation, float seconds);

    [[nodiscard]] math::Vec2 getVelocity() const;
    void setVelocity(math::Vec2 value);
    [[nodiscard]] float getAngularVelocity() const;
    void setAngularVelocity(float value);

    // Returns the velocity of a world point that moves with the body, which includes the spin of the body, such as the speed of the ground under the feet of a character.
    [[nodiscard]] math::Vec2 getVelocityAt(math::Vec2 point) const;

    // The mass, center of mass and inertia come from the density and area of the shapes until they are set, and `resetMassData` computes them from the shapes again. Setting the mass alone scales the inertia with it. Throws `std::invalid_argument` for a negative or infinite mass or inertia.
    [[nodiscard]] MassData getMassData() const;
    void setMassData(const MassData& value);
    void resetMassData();
    [[nodiscard]] float getMass() const;
    void setMass(float value);
    [[nodiscard]] math::Vec2 getCenterOfMass() const;
    void setCenterOfMass(math::Vec2 value);
    [[nodiscard]] float getInertia() const;
    void setInertia(float value);
    [[nodiscard]] math::Vec2 getWorldCenter() const;

    void applyForce(math::Vec2 force, std::optional<math::Vec2> point = std::nullopt);
    void applyImpulse(math::Vec2 impulse, std::optional<math::Vec2> point = std::nullopt);
    void applyTorque(float torque);
    void applyAngularImpulse(float impulse);

    [[nodiscard]] float getLinearDamping() const;
    void setLinearDamping(float value);
    [[nodiscard]] float getAngularDamping() const;
    void setAngularDamping(float value);
    [[nodiscard]] float getGravityScale() const;
    void setGravityScale(float value);
    [[nodiscard]] bool isFixedRotation() const;
    void setFixedRotation(bool value);
    [[nodiscard]] bool isBullet() const;
    void setBullet(bool value);
    [[nodiscard]] bool isAwake() const;
    void setAwake(bool value);
    [[nodiscard]] bool isSleepEnabled() const;
    void setSleepEnabled(bool value);
    [[nodiscard]] float getSleepThreshold() const;
    void setSleepThreshold(float value);
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool value);

    // Lets the body pass through every one-way platform for `seconds`, and through a platform it is still inside after that until it leaves it, which drops a character through the platform it stands on and onto the next one. Throws `std::invalid_argument` for a negative time.
    void dropThrough(float seconds);

    Shape addBox(math::Vec2 size, const Shape::Options& options = {});
    Shape addCircle(float radius, const Shape::Options& options = {});
    Shape addCapsule(math::Vec2 first, math::Vec2 second, float radius, const Shape::Options& options = {});
    Shape addSegment(math::Vec2 first, math::Vec2 second, const Shape::Options& options = {});

    // Adds any simple polygon. Convex outlines with at most eight points become one shape, and other outlines are split into convex pieces of at most eight points.
    std::vector<Shape> addPolygon(std::span<const math::Vec2> points, const Shape::Options& options = {});

    // Adds a one-sided chain of segments, which suits terrain outlines because bodies slide over its joints without catching. Each segment collides on its left as seen on screen when walking from one point to the next, so a loop listed counter-clockwise on screen holds bodies inside it. A loop closes the outline and needs at least four points. An open chain needs at least two points and collides along every segment between them, because the world extends its first and last segments with the points that smooth the contacts at its ends.
    std::vector<Shape> addChain(std::span<const math::Vec2> points, bool loop, const Shape::Options& options = {});

    [[nodiscard]] std::vector<Shape> getShapes() const;

    // Returns the contacts of the shapes of the body that touch now.
    [[nodiscard]] std::vector<Contact> getContacts() const;

    // Returns the outlines of every shape in world space for drawing, as `Shape::getOutline` makes them, with the segments of each chain joined into one outline that a loop closes.
    [[nodiscard]] std::vector<Shape::Outline> getOutlines() const;
    void destroy();

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] World* getWorld() const noexcept {
        return world;
    }
    [[nodiscard]] bool operator==(const Body& other) const noexcept {
        return worldHandle == other.worldHandle && id == other.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 3> kTypeNames;

    [[nodiscard]] std::uint64_t checkedId() const;
    // Applies the options that Box2D sets after a shape exists.
    [[nodiscard]] Shape finishShape(std::uint64_t shapeId, const Shape::Options& options);
    [[nodiscard]] Shape::Outline getChainOutline(std::uint64_t chainId) const;

    World* world = nullptr;
    std::uint32_t worldHandle = 0;
    std::uint64_t id = 0;
};

} // namespace haylen::physics2d
