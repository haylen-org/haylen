#pragma once

#include <vector>

#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/Program.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/RichTextLayout.hpp"
#include "haylen/text/TextLayout.hpp"
#include "haylen/text/TextStyle.hpp"

namespace haylen::graphics2d {

// Turns laid out text into the instances the renderer draws. Glyphs of distance field fonts go through the text program with their outline, weight, skew and softness, while bitmap glyphs, images and filled boxes go through the sprite program. Consecutive instances of one program and texture share a batch, and batches keep the drawing order, so backgrounds, glows and shadows lie under the glyphs.
class TextPainter final {
  public:
    struct Batch {
        Program program = Program::Sprite;
        graphics::Texture texture;
        std::vector<GpuInstance> instances;
    };

    explicit TextPainter(graphics::Texture whiteTexture);

    void paintText(text::Font& font, const text::TextLayout& layout, math::Vec2 position, const text::TextStyle& style);

    // Paints rich text with the top-left of its block at the position, scaled from that corner and with every color multiplied by the tint.
    void paintRichText(const text::RichTextLayout& layout, math::Vec2 position, math::Vec2 scale = {1.0F, 1.0F}, math::Color tint = math::Color::white());

    [[nodiscard]] const std::vector<Batch>& getBatches() const noexcept {
        return batches;
    }

  private:
    // A glyph leans around its baseline, so its pivot sits on the baseline at its left edge.
    [[nodiscard]] SpriteInstance placeGlyph(const text::RichTextLayout::Glyph& glyph, math::Vec2 offset, math::Color color, math::Color flash) const noexcept;
    [[nodiscard]] math::Vec2 place(math::Vec2 local) const noexcept;

    void add(Program program, const graphics::Texture& texture, const GpuInstance& instance);
    void paintBoxes(const text::RichTextLayout& layout, bool underText);
    void paintGlows(const text::RichTextLayout& layout);
    void paintShadows(const text::RichTextLayout& layout);
    void paintGlyphs(const text::RichTextLayout& layout);

    graphics::Texture white;
    std::vector<Batch> batches;
    math::Vec2 blockPosition{};
    math::Vec2 blockScale{1.0F, 1.0F};
    math::Color blockTint = math::Color::white();
};

} // namespace haylen::graphics2d
