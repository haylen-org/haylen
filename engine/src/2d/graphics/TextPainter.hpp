#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/Program.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::graphics2d {

// Turns laid out text into the instances the renderer draws. Glyphs of distance field fonts go through the text program with their outline, weight, skew and softness, while bitmap glyphs, images and filled boxes go through the sprite program. Consecutive instances of one program and texture share a batch, and batches keep the drawing order, so backgrounds, glows and shadows lie under the glyphs.
class TextPainter final {
  public:
    struct Batch {
        Program program = Program::Sprite;
        graphics::Texture texture;
        std::vector<GpuInstance> instances;
    };

    // The pixels of a destination in the units of a canvas that does not turn: a point of the canvas lands on the pixel `point * scale + offset`.
    struct PixelGrid {
        math::Vec2 scale{1.0F, 1.0F};
        math::Vec2 offset{};

        [[nodiscard]] float snapX(float x) const noexcept;
        [[nodiscard]] float snapY(float y) const noexcept;
    };

    explicit TextPainter(graphics::Texture whiteTexture);

    // Paints a layout of plain text in the color, outline and shadow of the style, stretched and turned by the style as one piece around the position, where its anchor lands. With a grid, text that does not turn puts the left edge of its block and the baselines of its glyphs on whole pixels.
    void paintText(const text::Layout& layout, math::Vec2 position, const text::Style& style, const std::optional<PixelGrid>& grid = std::nullopt);

    // Paints rich text with the top-left of its block at the position, scaled from that corner and with every color multiplied by the tint.
    void paintRichText(const text::Layout& layout, math::Vec2 position, math::Vec2 scale = {1.0F, 1.0F}, math::Color tint = math::Color::white());

    [[nodiscard]] const std::vector<Batch>& getBatches() const noexcept {
        return batches;
    }

  private:
    // The layers plain text draws, from the bottom up.
    enum class Layer : std::uint8_t {
        Shadow,
        Outline,
        Fill,
    };

    // A glyph leans around its baseline, so its pivot sits on the baseline at its left edge.
    [[nodiscard]] SpriteInstance placeGlyph(const text::Layout::Glyph& glyph, math::Vec2 offset, math::Color color, math::Color flash) const noexcept;
    [[nodiscard]] math::Vec2 place(math::Vec2 local) const noexcept;

    void add(Program program, const graphics::Texture& texture, const GpuInstance& instance);
    void paintPlainGlyphs(const text::Layout& layout, const text::Style& style, Layer layer);
    void paintBoxes(const text::Layout& layout, bool underText);
    void paintGlows(const text::Layout& layout);
    void paintShadows(const text::Layout& layout);
    void paintOutlines(const text::Layout& layout);
    void paintGlyphs(const text::Layout& layout);

    graphics::Texture white;
    std::vector<Batch> batches;
    math::Vec2 blockPosition{};
    math::Vec2 blockOrigin{};
    math::Vec2 blockScale{1.0F, 1.0F};
    float blockRotation = 0.0F;
    math::Color blockTint = math::Color::white();
    std::optional<PixelGrid> pixels;
};

} // namespace haylen::graphics2d
