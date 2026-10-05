#pragma once

#include <array>
#include <cstdint>

#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteEffect.hpp"
#include "haylen/2d/graphics/SpriteFlip.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
struct TextureResource;
}

namespace haylen::graphics2d {

// One sprite or glyph as the sprite and text shaders read it from the instance buffer. The four parameter bytes hold the flip bits of a sprite in the first byte, and the outline, weight and softness of a glyph drawn by the text program, while the third byte is the skew of both.
struct GpuInstance {
    // How a glyph looks beyond its color: the outline, weight and softness in distance field units and the skew as a horizontal shift per unit of height above the baseline.
    struct TextParameters {
        float outline = 0.0F;
        float weight = 0.0F;
        float skew = 0.0F;
        float softness = 0.0F;
    };

    float position[2];
    float size[2];
    std::uint16_t uv[4];
    std::uint32_t color;
    std::uint32_t flash;
    float rotation;
    std::uint8_t parameters[4];
    float pivot[2];

    [[nodiscard]] static GpuInstance make(const graphics::TextureResource& texture, const SpriteInstance& sprite) noexcept;

    // Packs a glyph, whose pivot lies on its baseline so the skew leans it around the baseline.
    [[nodiscard]] static GpuInstance makeGlyph(const graphics::TextureResource& texture, const SpriteInstance& glyph, const TextParameters& text) noexcept;

    // Packs the part colors of a recolored sprite into the record that follows its own, which the recolor program reads at the same instance.
    [[nodiscard]] static GpuInstance makeParts(const PartColors& colors) noexcept;

    // Packs the effect of a sprite into the record that follows its own, which the effect program reads at the same instance. The reach grows the quad of the sprite by the outline and the glow, as a share of its source of `sourceSize` pixels.
    [[nodiscard]] static GpuInstance makeEffect(const SpriteEffect& effect, math::Vec2 sourceSize) noexcept;

    // Returns the corners of the quad on its canvas from the top-left one clockwise, where the vertex stage places them, without the skew of italic glyphs.
    [[nodiscard]] std::array<math::Vec2, 4> getCorners() const noexcept;

  private:
    // The dissolve, outline and glow colors, the dissolve, its edge and the size of its noise, the reach of the quad and the outline and glow widths in pixels.
    struct Effect {
        std::uint32_t colors[3];
        std::uint8_t amounts[4];
        float reach[2];
        float widths[2];
        std::uint8_t unused[16];
    };

    // The colors of the red, green, blue and yellow parts in the first bytes of a record.
    struct Parts {
        std::uint32_t colors[4];
        std::uint8_t unused[32];
    };

    // The outline and the softness reach half the distance field, and the weight and the skew are signed bytes, so zeroed parameters leave a quad untouched.
    static constexpr float kUnsignedRange = 0.5F;
    static constexpr float kWeightScale = 256.0F;
    static constexpr float kSkewScale = 127.0F;
    static constexpr float kMaximumReach = 0.48F;

    [[nodiscard]] static std::uint16_t unorm16(float value) noexcept;
    [[nodiscard]] static std::uint8_t flipBits(SpriteFlip flip) noexcept;
    [[nodiscard]] static std::uint8_t unsignedByte(float value) noexcept;
    [[nodiscard]] static std::uint8_t signedByte(float value, float scale) noexcept;
};

} // namespace haylen::graphics2d
