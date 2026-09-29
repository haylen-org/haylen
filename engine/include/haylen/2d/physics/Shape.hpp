#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class Body;
class World;

// Handle to one collision shape. Handles stay cheap to copy, report false from isValid once the shape or its body is destroyed, and throw std::logic_error when used after that.
class Shape final {
  public:
    enum class Kind : std::uint8_t {
        Circle,
        Capsule,
        Segment,
        Polygon,
        // One segment of a chain, which collides on one side only.
        ChainSegment,
    };

    // A closed outline joins its last point back to the first one, and an open outline is a line through its points.
    struct Outline {
        std::vector<math::Vec2> points;
        bool closed = false;
    };

    // A tangent speed turns the surface into a conveyor. A one-way direction makes the shape a platform that only blocks bodies on the side the direction points to.
    struct Options {
        float density = 1.0F;
        float friction = 0.6F;
        float restitution = 0.0F;
        CollisionFilter filter{};
        bool sensor = false;
        math::Vec2 offset{};
        float rotation = 0.0F;
        float tangentSpeed = 0.0F;
        std::optional<math::Vec2> oneWay;
    };

    Shape() = default;
    // A handle remembers the generation of its world, so it turns invalid once that world is destroyed, even after a new world reuses its slot.
    Shape(World* owner, std::uint64_t handle) noexcept;

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Body getBody() const;
    [[nodiscard]] bool isSensor() const;
    [[nodiscard]] Kind getKind() const;

    // Returns the points that place the shape in the space of its body: the center of a circle, the two centers of a capsule, the two ends of a segment or of a chain segment, or the corners of a polygon in order around it.
    [[nodiscard]] std::vector<math::Vec2> getPoints() const;

    // Returns the points of getPoints in world space, where the body is now.
    [[nodiscard]] std::vector<math::Vec2> getWorldPoints() const;

    // Returns the radius of a circle or a capsule, and 0 for the other kinds.
    [[nodiscard]] float getRadius() const;

    // Returns the outline of the shape in world space for drawing: the corners of a polygon, a circle or a capsule traced with short straight steps, or the line of a segment.
    [[nodiscard]] Outline getOutline() const;
    [[nodiscard]] CollisionFilter getFilter() const;
    void setFilter(const CollisionFilter& value);

    // Returns the bounds Box2D tracks for the shape, which include its small collision margin.
    [[nodiscard]] math::Rect getBounds() const;
    void destroy();

    // Moves touching bodies along the surface at this speed in world units per second, clockwise around the shape as seen on screen, so a positive speed carries bodies on top of a platform to the right.
    [[nodiscard]] float getTangentSpeed() const;
    void setTangentSpeed(float value);

    // A one-way shape only blocks bodies that touch it from the side its direction points to. The direction is in the space of the body and turns with it, so {0, -1} on an unrotated body makes a platform that bodies jump through from below and land on from above. No direction makes the shape solid again.
    [[nodiscard]] std::optional<math::Vec2> getOneWay() const;
    void setOneWay(std::optional<math::Vec2> value);

    [[nodiscard]] std::uint64_t getId() const noexcept {
        return id;
    }
    [[nodiscard]] bool operator==(const Shape& other) const noexcept {
        return worldHandle == other.worldHandle && id == other.id;
    }

  private:
    static const std::array<std::pair<std::string_view, Kind>, 5> kKindNames;

    // Round outlines take one step for every two world units of radius, within these bounds for a whole turn.
    static constexpr int kMinimumTurnSteps = 16;
    static constexpr int kMaximumTurnSteps = 96;

    [[nodiscard]] std::uint64_t checkedId() const;
    [[nodiscard]] std::vector<math::Vec2> readPoints(bool inWorld) const;

    World* world = nullptr;
    std::uint32_t worldHandle = 0;
    std::uint64_t id = 0;
};

} // namespace haylen::physics2d
