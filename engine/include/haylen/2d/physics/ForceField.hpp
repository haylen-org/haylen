#pragma once

#include <array>
#include <cstddef>
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

// An area of a world that pushes the dynamic bodies inside it every step, in C++ without a call per body: radial fields pull toward their center, like magnets, attractors and the gravity of planets, or push away with a negative strength, directional fields push one way like wind and conveyors of air, vortex fields swirl, and buoyancy fields are water that floats bodies by the area they have under the surface and drags them toward its flow. Applying a field never wakes a body, so floating and resting bodies fall asleep, and changing the field wakes the bodies in it. The field acts every step of its world until it is destroyed, so it must go before its world.
class ForceField final {
  public:
    enum class Kind : std::uint8_t {
        Radial,
        Directional,
        Vortex,
        Buoyancy,
    };

    // How the strength fades with the distance from the center: not at all, linearly to zero at the radius, or with the inverse square of the distance beyond `minDistance`, like gravity.
    enum class Falloff : std::uint8_t {
        None,
        Linear,
        InverseSquare,
    };

    // The area is a circle of `radius` around the position when the radius is positive, a rectangle of `size` centered on it when the size is not zero, and otherwise the polygon of `points` around it. The strength is an acceleration in world units per second squared, the same for every body, or a force when `acceleration` is false, which pushes light bodies further. Radial fields pull toward the center and vortex fields turn clockwise on screen with a positive strength. Drag slows bodies toward the flow by that share of their speed every second. Buoyancy fields float bodies with the density of their water in kilograms per square meter and need a convex area whose top is the surface.
    struct Options {
        Kind kind = Kind::Radial;
        math::Vec2 position{};
        float radius = 0.0F;
        math::Vec2 size{};
        std::vector<math::Vec2> points;
        float strength = 0.0F;
        math::Vec2 direction{1.0F, 0.0F};
        Falloff falloff = Falloff::None;
        float minDistance = 0.0F;
        bool acceleration = true;
        float density = 1.0F;
        float linearDrag = 0.0F;
        float angularDrag = 0.0F;
        math::Vec2 flow{};
        CollisionFilter filter{};
        bool enabled = true;
    };

    // Throws `std::invalid_argument` for an area without size, a polygon with fewer than three points, a buoyancy area that is not convex, a zero direction, negative drags, density or minimum distance, or an inverse square falloff without a minimum distance.
    ForceField(World& owner, const Options& settings);
    ~ForceField();

    ForceField(const ForceField&) = delete;
    ForceField& operator=(const ForceField&) = delete;

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;
    [[nodiscard]] static std::optional<Falloff> falloffFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view falloffName(Falloff value) noexcept;

    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    void setEnabled(bool value);
    void setStrength(float value);
    void setPosition(math::Vec2 value);
    void setDirection(math::Vec2 value);
    void setFlow(math::Vec2 value);
    void setDensity(float value);
    void setLinearDrag(float value);
    void setAngularDrag(float value);

    // Stops the field for good, which also happens when it is destroyed.
    void destroy();
    [[nodiscard]] bool isValid() const noexcept {
        return hook != 0;
    }

    // Returns the bounds of the area and the number of bodies the field pushed in the last step.
    [[nodiscard]] math::Rect getBounds() const;
    [[nodiscard]] std::size_t getBodyCount() const noexcept {
        return bodyCount;
    }

  private:
    struct Target {
        std::uint64_t body = 0;
        std::uint64_t shape = 0;
    };

    static const std::array<std::pair<std::string_view, Kind>, 4> kKindNames;
    static const std::array<std::pair<std::string_view, Falloff>, 3> kFalloffNames;

    // Circles and capsules become polygons with this many points on each half turn when they are clipped against water.
    static constexpr int kRoundSteps = 8;

    void check() const;
    [[nodiscard]] bool contains(math::Vec2 point) const;
    [[nodiscard]] float fade(float distance) const noexcept;
    // Lists the dynamic shapes whose bounds overlap the area.
    void gather();
    void wake();
    void update(float deltaSeconds);
    void push(const Body& body);
    void floatShape(const Body& body, std::uint64_t shape);
    [[nodiscard]] std::vector<math::Vec2> outlineOf(std::uint64_t shape) const;
    // Clips a polygon against the convex area of the field.
    [[nodiscard]] std::vector<math::Vec2> clip(std::vector<math::Vec2> polygon) const;

    World& world;
    Options options;
    std::vector<math::Vec2> area;
    std::vector<Target> targets;
    std::size_t bodyCount = 0;
    std::uint64_t hook = 0;
};

} // namespace haylen::physics2d
