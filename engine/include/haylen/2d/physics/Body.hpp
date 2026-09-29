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

// Handle to a rigid body. Every length is in world units and every mass in kilograms, so forces and impulses scale with world units and torques and angular impulses with squared world units, and the world converts them all to meters for the solver.
class Body final {
  public:
    enum class Type : std::uint8_t {
        Static,
        Kinematic,
        Dynamic,
    };

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
        bool sleepEnabled = true;
    };

    Body() = default;
    Body(World* owner, std::uint64_t handle) noexcept : world(owner), id(handle) {}

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Type getType() const;
    void setType(Type value);
    [[nodiscard]] math::Vec2 getPosition() const;
    [[nodiscard]] float getRotation() const;
    void setTransform(math::Vec2 position, float rotation);
    [[nodiscard]] math::Vec2 getVelocity() const;
    void setVelocity(math::Vec2 value);
    [[nodiscard]] float getAngularVelocity() const;
    void setAngularVelocity(float value);
    [[nodiscard]] float getMass() const;

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
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool value);

    Shape addBox(math::Vec2 size, const Shape::Options& options = {});
    Shape addCircle(float radius, const Shape::Options& options = {});
    Shape addCapsule(math::Vec2 first, math::Vec2 second, float radius, const Shape::Options& options = {});
    Shape addSegment(math::Vec2 first, math::Vec2 second, const Shape::Options& options = {});

    // Adds any simple polygon. Convex outlines with at most eight points become one shape, and other outlines are split into convex pieces of at most eight points.
    std::vector<Shape> addPolygon(std::span<const math::Vec2> points, const Shape::Options& options = {});

    // Adds a one-sided chain of segments, which suits terrain outlines. Each segment collides on its left as seen on screen when walking from one point to the next, so a loop listed counter-clockwise on screen holds bodies inside it. Loops close the outline.
    std::vector<Shape> addChain(std::span<const math::Vec2> points, bool loop, const Shape::Options& options = {});

    [[nodiscard]] std::vector<Shape> getShapes() const;

    // Returns the outlines of every shape in world space for drawing, as Shape::getOutline makes them, with the segments of each chain joined into one outline that a loop closes.
    [[nodiscard]] std::vector<Shape::Outline> getOutlines() const;
    void destroy();

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] World* getWorld() const noexcept {
        return world;
    }
    [[nodiscard]] bool operator==(const Body& other) const noexcept {
        return id == other.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Type>, 3> kTypeNames;

    [[nodiscard]] std::uint64_t checkedId() const;
    // Applies the options that Box2D sets after a shape exists.
    [[nodiscard]] Shape finishShape(std::uint64_t shapeId, const Shape::Options& options);
    [[nodiscard]] Shape::Outline getChainOutline(std::uint64_t chainId) const;

    World* world = nullptr;
    std::uint64_t id = 0;
};

} // namespace haylen::physics2d
