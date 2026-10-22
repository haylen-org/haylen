#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lighting2d {

// Casts the 1D shadow maps of lights on the CPU. A point or spot light stores the distance of the nearest occluder in every direction around it, and a directional light the depth of the nearest occluder along the light at every point across it, both as fractions of their range. The light pass reads one row per shadowed light: the map of `kResolution` texels, then smaller copies of it, each half as wide as the one before, whose texels keep the nearest occluder of the two texels they cover, so a wide soft edge reads few texels and never misses an occluder.
class ShadowMap final {
  public:
    static constexpr int kResolution = 1024;
    static constexpr int kLevels = 7;
    static constexpr int kRowTexels = 2 * kResolution;

    // Depth of the texels that no occluder reaches.
    static constexpr float kClear = 1.0e9F;

    struct Segment {
        math::Vec2 from{};
        math::Vec2 to{};
        std::uint32_t mask = 1;
        Occluder::Cull cull = Occluder::Cull::Disabled;
    };

    // Where the texels of a directional map lie across the light, and where its depths start along it, both in world units.
    struct Axis {
        float acrossStart = 0.0F;
        float acrossSpan = 1.0F;
        float alongStart = 0.0F;
        float alongSpan = 1.0F;
    };

    // Adds the edges of an occluder in world coordinates.
    static void appendSegments(const Occluder& occluder, std::vector<Segment>& segments);

    // Fills a row of `kRowTexels` texels for the light from the segments that cast its shadows, the map and its smaller copies, and returns the axis of directional maps, which cover the bounds.
    static Axis cast(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row);

    // Returns how far a point may lie behind the nearest occluder depth and still count as lit, as a fraction of the range, which keeps occluder edges from shadowing themselves.
    [[nodiscard]] static float getBias(const Light& light, const Axis& axis) noexcept;

    // Returns the distance the depths of a point or spot light are fractions of: its radius times its largest scale.
    [[nodiscard]] static float getRange(const Light& light) noexcept;

    // Tells whether a segment that casts the light's shadows stands between the light and a point, which a row approximates with one direction per texel.
    [[nodiscard]] static bool blocks(const Light& light, std::span<const Segment> segments, math::Vec2 point) noexcept;

  private:
    static constexpr float kBiasUnits = 1.5F;
    static constexpr float kEdgeOnUnits = 1.0e-3F;

    // A segment casts shadows when its mask shares a bit with the shadow mask and the light does not see it wound the way its cull mode skips.
    [[nodiscard]] static bool casts(const Light& light, const Segment& segment) noexcept;

    // Tells whether a point light, at the origin of the segment's coordinates, lies on the line of the segment, such as on a corner of its occluder, so it sees the segment edge-on and the segment blocks no direction.
    [[nodiscard]] static bool isEdgeOn(math::Vec2 from, math::Vec2 to) noexcept;
    static void castRadial(const Light& light, std::span<const Segment> segments, std::span<float> row);
    static void reduce(std::span<float> row) noexcept;
    static Axis castParallel(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row);
};

} // namespace haylen::lighting2d
