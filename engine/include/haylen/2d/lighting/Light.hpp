#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lighting2d {

// A light of a lit canvas. Point lights shine around their position up to their radius through their texture, the engine's radial falloff when it has none, spot lights shine the same way inside a cone, and directional lights cover the whole canvas from their direction. Draws take the light when their light mask shares a bit with its item mask and their layer lies in its layer range, and occluders whose mask shares a bit with its shadow mask cast its shadows.
struct Light {
    enum class Type : std::uint8_t {
        Point,
        Spot,
        Directional,
    };

    // Added lights brighten the light map, subtracted lights darken it, and mixed lights replace it by their strength.
    enum class Blend : std::uint8_t {
        Add,
        Subtract,
        Mix,
    };

    // Shadow edges take one sample of the shadow map, or 5 or 13 samples spread by the smoothness for softer edges.
    enum class ShadowFilter : std::uint8_t {
        None,
        Pcf5,
        Pcf13,
    };

    // Lit canvases tell layers apart from the lowest to the highest of these, and count layers beyond them as the nearest one.
    static constexpr int kLowestLayer = -128;
    static constexpr int kHighestLayer = 127;

    // Spot cones whose inner and outer angles match still fade over this difference of cosines, which keeps their edge defined.
    static constexpr float kConeEdge = 0.0001F;

    Type type = Type::Point;
    math::Vec2 position{};
    float radius = 160.0F;
    math::Color color = math::Color::white();

    // Multiplies the color like its alpha does, and values above 1 brighten the scene beyond its unlit colors where the light map is in floating point.
    float intensity = 1.0F;

    // Turns the texture of point lights and points spot and directional lights, where 0 points right and a quarter turn points down.
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    graphics::Texture texture;

    // Full angles of the cone of spot lights, which shine fully inside the inner angle and fade out at the outer angle.
    float innerAngle = 0.5F;
    float outerAngle = 1.0F;

    // Height of the light above the canvas, which sets the angle light reaches normal-mapped draws at.
    float height = 0.0F;
    bool enabled = true;
    Blend blend = Blend::Add;
    std::uint8_t itemMask = 1;
    int layerMin = std::numeric_limits<int>::min();
    int layerMax = std::numeric_limits<int>::max();

    bool shadows = false;
    ShadowFilter shadowFilter = ShadowFilter::None;

    // Light that shadowed pixels still receive, where the alpha sets how dark the shadow is.
    math::Color shadowColor = math::Color::black();

    // Widens the spread of the filter samples, in shadow map texels beyond the first.
    float shadowSmoothness = 0.0F;
    std::uint32_t shadowMask = 1;

    // Resolve the names "point", "spot" and "directional", "add", "subtract" and "mix", and "none", "pcf5" and "pcf13".
    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
    [[nodiscard]] static std::optional<Blend> blendFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view blendName(Blend value) noexcept;
    [[nodiscard]] static std::optional<ShadowFilter> shadowFilterFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view shadowFilterName(ShadowFilter value) noexcept;

    // The radial falloff of lights without a texture, 1 at the center and 0 at the radius, for a distance given as a fraction of the radius.
    [[nodiscard]] static float falloff(float distance) noexcept;

    // Returns the light map value at a point of the canvas plane that the ambient color and the lights leave for draws with the light mask and the layer, in order and without normal maps or shadows, as the light pass computes it. Textured lights count with the radial falloff.
    [[nodiscard]] static math::Color illuminate(math::Color ambient, std::span<const Light> lights, math::Vec2 point, std::uint8_t lightMask = 1, int layer = 0) noexcept;

    // Throws std::invalid_argument when a value is out of its range, such as a negative intensity, a spot cone whose inner angle exceeds its outer angle or a layer range that is empty.
    void validate() const;

    // Tells whether the light reaches draws with the light mask and the layer.
    [[nodiscard]] bool affects(std::uint8_t lightMask, int layer) const noexcept;

    // Returns how strongly the light reaches a point, from 0 to 1, through the falloff and the cone of point and spot lights. Directional lights reach every point fully.
    [[nodiscard]] float getStrengthAt(math::Vec2 point) const noexcept;

    // Returns the light map value after the light draws over the value below at a point, with its blend mode.
    [[nodiscard]] math::Color apply(math::Color below, math::Vec2 point) const noexcept;

    // Tells whether an occluder whose mask shares a bit with the shadow mask stands between the light and a point, following the cull mode of each occluder the way the shadow map does. Lights without shadows never shadow a point.
    [[nodiscard]] bool isShadowedAt(math::Vec2 point, std::span<const Occluder> occluders) const;

  private:
    static const std::array<std::string_view, 3> kTypeNames;
    static const std::array<std::string_view, 3> kBlendNames;
    static const std::array<std::string_view, 3> kShadowFilterNames;
};

} // namespace haylen::lighting2d
