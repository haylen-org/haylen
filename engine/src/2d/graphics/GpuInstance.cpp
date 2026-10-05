#include "2d/graphics/GpuInstance.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "graphics/TextureResource.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::graphics2d {

std::uint16_t GpuInstance::unorm16(float value) noexcept {
    return static_cast<std::uint16_t>(std::clamp(value, 0.0F, 1.0F) * 65535.0F + 0.5F);
}

// Converts a distance of zero or more to the bits of a half float, rounding to the nearest and keeping it within the largest finite half.
std::uint16_t GpuInstance::toHalf(float value) noexcept {
    const float clamped = std::clamp(value, 0.0F, kLargestHalf);
    if (clamped < kSmallestNormalHalf) {
        return static_cast<std::uint16_t>(std::lround(clamped * kSubnormalHalfSteps));
    }
    const auto bits = std::bit_cast<std::uint32_t>(clamped);
    const std::uint32_t rounded = bits + 0x0FFFU + ((bits >> 13U) & 1U);
    return static_cast<std::uint16_t>((rounded - 0x38000000U) >> 13U);
}

std::uint32_t GpuInstance::scaleAlpha(std::uint32_t color, float amount) noexcept {
    const float alpha = static_cast<float>(color >> 24U) * amount;
    return (color & 0x00FFFFFFU) | (static_cast<std::uint32_t>(std::lround(std::clamp(alpha, 0.0F, 255.0F))) << 24U);
}

// The sprite shader reads bit 0 as a horizontal flip, bit 1 as a vertical flip and bit 2 as a diagonal flip.
std::uint8_t GpuInstance::flipBits(SpriteFlip flip) noexcept {
    return static_cast<std::uint8_t>((flip.horizontal ? 1U : 0U) | (flip.vertical ? 2U : 0U) | (flip.diagonal ? 4U : 0U));
}

std::uint8_t GpuInstance::unsignedByte(float value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value / kUnsignedRange, 0.0F, 1.0F) * 255.0F));
}

// Stores a signed value as the two's complement byte the shaders decode.
std::uint8_t GpuInstance::signedByte(float value, float scale) noexcept {
    const long step = std::clamp(std::lround(value * scale), -127L, 127L);
    return static_cast<std::uint8_t>(step < 0 ? step + 256 : step);
}

GpuInstance GpuInstance::make(const graphics::TextureResource& texture, const SpriteInstance& sprite) noexcept {
    const auto width = static_cast<float>(texture.width);
    const auto height = static_cast<float>(texture.height);
    const math::Rect source = sprite.source.isEmpty() ? math::Rect{0.0F, 0.0F, width, height} : sprite.source;

    float top = source.getTop() / height;
    float bottom = source.getBottom() / height;
    if (texture.flipped) {
        top = 1.0F - top;
        bottom = 1.0F - bottom;
    }

    return {
        .position = {sprite.position.x, sprite.position.y},
        .size = {sprite.size.x, sprite.size.y},
        .uv = {unorm16(source.getLeft() / width), unorm16(top), unorm16(source.getRight() / width), unorm16(bottom)},
        .color = sprite.color.toRgba8(),
        .flash = sprite.flash.toRgba8(),
        .rotation = sprite.rotation,
        .parameters = {flipBits(sprite.flip), 0, 0, 0},
        .pivot = {sprite.pivot.x, sprite.pivot.y},
    };
}

// The outline, a positive weight and the softness shrink together into the reach their bytes hold, and the text shader shrinks them again to what the field of the glyph reaches at its size on screen.
GpuInstance GpuInstance::makeGlyph(const graphics::TextureResource& texture, const SpriteInstance& glyph, const TextParameters& text) noexcept {
    const float reach = std::max(text.weight, 0.0F) + text.outline + text.softness;
    const float fit = reach > kMaximumReach ? kMaximumReach / reach : 1.0F;
    GpuInstance packed = make(texture, glyph);
    packed.parameters[0] = unsignedByte(text.outline * fit);
    packed.parameters[1] = signedByte(text.weight > 0.0F ? text.weight * fit : text.weight, kWeightScale);
    packed.parameters[2] = signedByte(text.skew, kSkewScale);
    packed.parameters[3] = unsignedByte(text.softness * fit);
    return packed;
}

GpuInstance GpuInstance::makeParts(const PartColors& colors) noexcept {
    static_assert(sizeof(Parts) == sizeof(GpuInstance));
    return std::bit_cast<GpuInstance>(Parts{.colors = {colors.red.toRgba8(), colors.green.toRgba8(), colors.blue.toRgba8(), colors.yellow.toRgba8()}, .unused = {}});
}

GpuInstance GpuInstance::makeEffect(const SpriteEffect& effect, math::Vec2 sourceSize) noexcept {
    static_assert(sizeof(Effect) == sizeof(GpuInstance));
    const float reach = std::max(effect.outlineWidth, effect.glowSize) + 1.0F;
    const auto byte = [](float value) { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F)); };
    return std::bit_cast<GpuInstance>(Effect{
        .colors = {effect.dissolveColor.toRgba8(), effect.outlineColor.toRgba8(), effect.glowColor.toRgba8()},
        .amounts = {byte(effect.dissolve), byte(effect.dissolveEdge), byte(effect.dissolveSize / SpriteEffect::kMaxReach), 0},
        .reach = {reach / std::max(sourceSize.x, 1.0F), reach / std::max(sourceSize.y, 1.0F)},
        .widths = {effect.outlineWidth, effect.glowSize},
        .unused = {},
    });
}

GpuInstance GpuInstance::makeShape(const Shape& shape, float margin) noexcept {
    static_assert(sizeof(ShapeRecord) == sizeof(GpuInstance));
    const math::Vec2 center = shape.bounds.getCenter();
    const math::Vec2 half = shape.bounds.getSize() * 0.5F;
    const float shorter = std::min(half.x, half.y);
    const auto share = [shorter](float value) { return unorm16(shorter > 0.0F ? value / shorter : 0.0F); };
    const float start = std::fmod(shape.startAngle, math::Math::kTau);
    return std::bit_cast<GpuInstance>(ShapeRecord{
        .center = {center.x, center.y},
        .halfSize = {half.x, half.y},
        .rotation = shape.rotation,
        .reach = {toHalf(shape.softness), toHalf(margin)},
        .colors = {shape.color.toRgba8(), shape.borderColor.toRgba8()},
        .form = {share(shape.borderWidth), unorm16(shape.sweep / math::Math::kTau), unorm16((start < 0.0F ? start + math::Math::kTau : start) / math::Math::kTau), 0},
        .radii = {share(shape.radii[0]), share(shape.radii[1]), share(shape.radii[2]), share(shape.radii[3])},
    });
}

void GpuInstance::fade(float amount) noexcept {
    color = scaleAlpha(color, amount);
}

void GpuInstance::fadeShape(float amount) noexcept {
    ShapeRecord record = std::bit_cast<ShapeRecord>(*this);
    for (std::uint32_t& value : record.colors) {
        value = scaleAlpha(value, amount);
    }
    *this = std::bit_cast<GpuInstance>(record);
}

std::array<math::Vec2, 4> GpuInstance::getCorners() const noexcept {
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    std::array<math::Vec2, 4> corners{math::Vec2{0.0F, 0.0F}, math::Vec2{1.0F, 0.0F}, math::Vec2{1.0F, 1.0F}, math::Vec2{0.0F, 1.0F}};
    for (math::Vec2& corner : corners) {
        const math::Vec2 local = (corner - math::Vec2{pivot[0], pivot[1]}) * math::Vec2{size[0], size[1]};
        corner = math::Vec2{position[0] + local.x * cosine - local.y * sine, position[1] + local.x * sine + local.y * cosine};
    }
    return corners;
}

} // namespace haylen::graphics2d
