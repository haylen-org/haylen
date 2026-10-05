#include "2d/graphics/LightInstance.hpp"

#include <algorithm>
#include <cmath>

#include "2d/lighting/ShadowMap.hpp"

namespace haylen::graphics2d {

// Directional lights cover the view of the canvas, and the others a quad of their reach turned with them.
LightInstance LightInstance::make(const LightDraw& draw, const math::Rect& bounds) noexcept {
    const lighting2d::Light& light = draw.light;
    const bool directional = light.type == lighting2d::Light::Type::Directional;
    const math::Vec2 center = directional ? bounds.getCenter() : light.position;
    const math::Vec2 half = directional ? bounds.getSize() * 0.5F : light.scale * light.radius;
    const math::Vec2 direction = math::Vec2::fromAngle(light.rotation);
    const float energy = light.intensity * light.color.a;
    const float outer = std::cos(light.outerAngle * 0.5F);
    const int samples = light.shadowFilter == lighting2d::Light::ShadowFilter::Pcf13 ? 13 : (light.shadowFilter == lighting2d::Light::ShadowFilter::Pcf5 ? 5 : 1);
    const int lowest = std::clamp(light.layerMin, lighting2d::Light::kLowestLayer, lighting2d::Light::kHighestLayer);
    const int highest = std::clamp(light.layerMax, lighting2d::Light::kLowestLayer, lighting2d::Light::kHighestLayer);
    return {
        .area = {center.x, center.y, half.x, half.y},
        .color = {light.color.r * energy, light.color.g * energy, light.color.b * energy, static_cast<float>(light.blend)},
        .shape = {static_cast<float>(light.type), lighting2d::ShadowMap::getRange(light), light.height, static_cast<float>(draw.shadowRow)},
        .cone = {direction.x, direction.y, std::max(std::cos(light.innerAngle * 0.5F), outer + lighting2d::Light::kConeEdge), outer},
        .origin = {light.position.x, light.position.y, static_cast<float>(light.itemMask), directional ? 0.0F : light.rotation},
        .range = {static_cast<float>(lowest), static_cast<float>(highest), static_cast<float>(samples), 1.0F + light.shadowSmoothness},
        .shadowColor = {light.shadowColor.r, light.shadowColor.g, light.shadowColor.b, light.shadowColor.a},
        .shadowMap = {static_cast<float>(lighting2d::ShadowMap::kResolution), lighting2d::ShadowMap::getBias(light, draw.axis), 0.0F, 0.0F},
        .shadowAxis = {draw.axis.acrossStart, draw.axis.acrossSpan, draw.axis.alongStart, draw.axis.alongSpan},
    };
}

} // namespace haylen::graphics2d
