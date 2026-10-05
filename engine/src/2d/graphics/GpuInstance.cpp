#include "2d/graphics/GpuInstance.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "graphics/TextureResource.hpp"

namespace haylen::graphics2d {

std::uint16_t GpuInstance::unorm16(float value) noexcept {
    return static_cast<std::uint16_t>(std::clamp(value, 0.0F, 1.0F) * 65535.0F + 0.5F);
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

} // namespace haylen::graphics2d
