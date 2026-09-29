#include "haylen/2d/lighting/Light.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/lighting/ShadowMap.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::lighting2d {

const std::array<std::string_view, 3> Light::kTypeNames = {"point", "spot", "directional"};
const std::array<std::string_view, 3> Light::kBlendNames = {"add", "subtract", "mix"};
const std::array<std::string_view, 3> Light::kShadowFilterNames = {"none", "pcf5", "pcf13"};

std::optional<Light::Type> Light::typeFromName(std::string_view name) noexcept {
    const auto found = std::find(kTypeNames.begin(), kTypeNames.end(), name);
    return found == kTypeNames.end() ? std::nullopt : std::optional(static_cast<Type>(found - kTypeNames.begin()));
}

std::string_view Light::typeName(Type value) noexcept {
    return kTypeNames[static_cast<std::size_t>(value)];
}

std::optional<Light::Blend> Light::blendFromName(std::string_view name) noexcept {
    const auto found = std::find(kBlendNames.begin(), kBlendNames.end(), name);
    return found == kBlendNames.end() ? std::nullopt : std::optional(static_cast<Blend>(found - kBlendNames.begin()));
}

std::string_view Light::blendName(Blend value) noexcept {
    return kBlendNames[static_cast<std::size_t>(value)];
}

std::optional<Light::ShadowFilter> Light::shadowFilterFromName(std::string_view name) noexcept {
    const auto found = std::find(kShadowFilterNames.begin(), kShadowFilterNames.end(), name);
    return found == kShadowFilterNames.end() ? std::nullopt : std::optional(static_cast<ShadowFilter>(found - kShadowFilterNames.begin()));
}

std::string_view Light::shadowFilterName(ShadowFilter value) noexcept {
    return kShadowFilterNames[static_cast<std::size_t>(value)];
}

float Light::falloff(float distance) noexcept {
    const float strength = 1.0F - math::Math::smoothstep(0.0F, 1.0F, distance);
    return strength * strength;
}

math::Color Light::illuminate(math::Color ambient, std::span<const Light> lights, math::Vec2 point, std::uint8_t lightMask, int layer) noexcept {
    math::Color value = ambient;
    for (const Light& light : lights) {
        if (light.affects(lightMask, layer)) {
            value = light.apply(value, point);
        }
    }
    return value;
}

void Light::validate() const {
    if (type != Type::Directional && !(radius > 0.0F && std::isfinite(radius))) {
        throw std::invalid_argument("A light radius must be positive.");
    }
    if (!(intensity >= 0.0F && std::isfinite(intensity))) {
        throw std::invalid_argument("A light intensity must be zero or positive.");
    }
    if (!(scale.x > 0.0F && scale.y > 0.0F)) {
        throw std::invalid_argument("A light scale must be positive.");
    }
    if (type == Type::Spot && !(innerAngle >= 0.0F && innerAngle <= outerAngle && outerAngle > 0.0F && outerAngle <= math::Math::kTau)) {
        throw std::invalid_argument("A spot light needs an outer angle up to a full turn and an inner angle from 0 to the outer angle.");
    }
    if (!(height >= 0.0F)) {
        throw std::invalid_argument("A light height must be zero or positive.");
    }
    if (layerMin > layerMax) {
        throw std::invalid_argument("A light layer range needs layerMin at most layerMax.");
    }
    if (!(shadowSmoothness >= 0.0F)) {
        throw std::invalid_argument("A light shadow smoothness must be zero or positive.");
    }
}

bool Light::affects(std::uint8_t lightMask, int layer) const noexcept {
    const int clamped = std::clamp(layer, kLowestLayer, kHighestLayer);
    return (lightMask & itemMask) != 0 && clamped >= std::clamp(layerMin, kLowestLayer, kHighestLayer) && clamped <= std::clamp(layerMax, kLowestLayer, kHighestLayer);
}

float Light::getStrengthAt(math::Vec2 point) const noexcept {
    if (type == Type::Directional) {
        return 1.0F;
    }

    const math::Vec2 offset = point - position;
    const math::Vec2 local = offset.rotated(-rotation) / (scale * radius);
    float strength = falloff(local.getLength());
    if (type == Type::Spot) {
        const float distance = offset.getLength();
        const float along = distance > 0.0F ? math::Vec2::dot(offset / distance, math::Vec2::fromAngle(rotation)) : 1.0F;
        const float outer = std::cos(outerAngle * 0.5F);
        strength *= math::Math::smoothstep(outer, std::max(std::cos(innerAngle * 0.5F), outer + kConeEdge), along);
    }
    return strength;
}

math::Color Light::apply(math::Color below, math::Vec2 point) const noexcept {
    if (!enabled) {
        return below;
    }

    const float strength = getStrengthAt(point);
    const float energy = intensity * color.a;
    const math::Color light{color.r * energy, color.g * energy, color.b * energy, below.a};
    switch (blend) {
    case Blend::Add:
        return {below.r + light.r * strength, below.g + light.g * strength, below.b + light.b * strength, below.a};
    case Blend::Subtract:
        return {below.r - light.r * strength, below.g - light.g * strength, below.b - light.b * strength, below.a};
    case Blend::Mix:
        return math::Color::lerp(below, light, std::clamp(strength, 0.0F, 1.0F));
    }
    return below;
}

bool Light::isShadowedAt(math::Vec2 point, std::span<const Occluder> occluders) const {
    for (const Occluder& occluder : occluders) {
        occluder.validate();
    }
    if (!shadows) {
        return false;
    }

    std::vector<ShadowMap::Segment> segments;
    for (const Occluder& occluder : occluders) {
        ShadowMap::appendSegments(occluder, segments);
    }
    return ShadowMap::blocks(*this, segments, point);
}

} // namespace haylen::lighting2d
