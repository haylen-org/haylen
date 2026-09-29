#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lighting2d {

// Casts the 1D shadow maps of lights on the CPU, the maps Godot renders on the GPU. A point or spot light stores the distance of the nearest occluder in every direction around it, and a directional light the depth of the nearest occluder along the light at every point across it, both as fractions of their range. The light pass reads one row per shadowed light.
class ShadowMap final {
  public:
    static constexpr int kResolution = 1024;

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

    // Fills a row of kResolution texels for the light from the segments that cast its shadows, and returns the axis of directional maps, which cover the bounds.
    static Axis cast(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row);

    // Returns how far a point may lie behind the nearest occluder depth and still count as lit, as a fraction of the range, which keeps occluder edges from shadowing themselves.
    [[nodiscard]] static float getBias(const Light& light, const Axis& axis) noexcept;

    // Returns the distance the depths of a point or spot light are fractions of: its radius times its largest scale.
    [[nodiscard]] static float getRange(const Light& light) noexcept;

    // Tells whether a row shadows a point, reading the one texel the light pass reads without filtering.
    [[nodiscard]] static bool isShadowed(const Light& light, std::span<const float> row, const Axis& axis, math::Vec2 point) noexcept;

    // Tells whether a segment that casts the light's shadows stands between the light and a point, which a row approximates with one direction per texel.
    [[nodiscard]] static bool blocks(const Light& light, std::span<const Segment> segments, math::Vec2 point) noexcept;

  private:
    static constexpr float kBiasUnits = 1.5F;

    // A segment casts shadows when its mask shares a bit with the shadow mask and the light does not see it wound the way its cull mode skips.
    [[nodiscard]] static bool casts(const Light& light, const Segment& segment) noexcept;
    static void castRadial(const Light& light, std::span<const Segment> segments, std::span<float> row);
    static Axis castParallel(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row);
};

} // namespace haylen::lighting2d
