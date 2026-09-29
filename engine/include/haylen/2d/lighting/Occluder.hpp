#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {
class Body;
class World;
} // namespace haylen::physics2d

namespace haylen::tiled {
class MapRenderer;
}

namespace haylen::lighting2d {

// A shape that blocks the light of lights with shadows. Its points are local to its position, rotation and scale, and a closed occluder also joins its last point to the first. Every edge casts shadows unless the cull mode skips the edges a light sees wound clockwise or counterclockwise on screen, so a polygon wound clockwise with counterclockwise culling stays lit itself and only darkens what lies behind it. Lights cast shadows from occluders whose mask shares a bit with their shadow mask.
struct Occluder {
    enum class Cull : std::uint8_t {
        Disabled,
        Clockwise,
        CounterClockwise,
    };

    std::vector<math::Vec2> points;
    bool closed = true;
    Cull cull = Cull::Disabled;
    std::uint32_t mask = 1;
    math::Vec2 position{};
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};

    // Resolves the names "disabled", "clockwise" and "counterClockwise".
    [[nodiscard]] static std::optional<Cull> cullFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view cullName(Cull value) noexcept;

    // Builds one occluder per shape of a body, in the space of the body at the position and rotation it has now: polygons, boxes, circles and capsules closed, and segments and chains open. Occluders of moving bodies follow them when they take the position and rotation of the body every frame.
    [[nodiscard]] static std::vector<Occluder> fromBody(const physics2d::World& world, const physics2d::Body& body);

    // Builds one occluder per object of the named object layer of a Tiled map, or of every object layer when the name is empty, in world coordinates: closed shapes closed and polylines open. Points and text have no outline and make no occluder.
    [[nodiscard]] static std::vector<Occluder> fromMap(const tiled::MapRenderer& map, std::string_view layer = {});

    // Throws std::invalid_argument when an open occluder has fewer than 2 points or a closed one fewer than 3.
    void validate() const;

    // Returns the points in world coordinates, through the scale, the rotation and the position.
    [[nodiscard]] std::vector<math::Vec2> getWorldPoints() const;

  private:
    static constexpr int kCurveSegments = 16;
    static const std::array<std::string_view, 3> kCullNames;
};

} // namespace haylen::lighting2d
