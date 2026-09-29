#include "2d/graphics/TextPainter.hpp"

#include <cmath>
#include <utility>

#include "graphics/TextureResource.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::graphics2d {

TextPainter::TextPainter(graphics::Texture whiteTexture) : white(std::move(whiteTexture)) {}

void TextPainter::add(Program program, const graphics::Texture& texture, const GpuInstance& instance) {
    if (batches.empty() || batches.back().program != program || batches.back().texture != texture) {
        batches.push_back({.program = program, .texture = texture});
    }
    batches.back().instances.push_back(instance);
}

// Plain text turns as one piece around the position, so every glyph turns around its baseline at a turned place. Shadows are the same glyphs drawn first, moved by the offset on the screen, in the shadow color with the blur as softness, and a bitmap shadow is the silhouette of its glyph.
void TextPainter::paintText(const text::TextLayout& layout, math::Vec2 position, const text::TextStyle& style, math::Vec2 scale) {
    blockPosition = position;
    blockOrigin = layout.size * style.anchor;
    blockScale = scale;
    blockRotation = style.rotation;
    blockTint = math::Color::white();
    if (style.shadowColor.a > 0.0F) {
        paintPlainGlyphs(layout, style, true);
    }
    paintPlainGlyphs(layout, style, false);
}

// A synthetic bold bitmap glyph draws a second copy a native pixel to the right.
void TextPainter::paintPlainGlyphs(const text::TextLayout& layout, const text::TextStyle& style, bool shadow) {
    for (const text::TextLayout::Glyph& glyph : layout.glyphs) {
        const text::TextLayout::Look& look = layout.looks[glyph.look];
        const bool field = look.font->isDistanceField();
        const float outline = look.font->toDistance(style.outlineWidth, look.size);
        math::Color color = style.color;
        math::Color flash = field && outline > 0.0F ? style.outlineColor : math::Color::transparent();
        GpuInstance::TextParameters parameters{.outline = outline, .weight = look.font->toDistance(look.weight, look.size), .skew = look.skew};
        if (shadow) {
            color = style.shadowColor;
            flash = field ? color.withAlpha(outline > 0.0F ? color.a : 0.0F) : color.withAlpha(1.0F);
            parameters.softness = look.font->toDistance(style.shadowBlur, look.size);
        }

        const graphics::Texture& page = look.font->getPage(glyph.page);
        const Program program = field ? Program::Text : Program::Sprite;
        const math::Vec2 moved = shadow ? style.shadowOffset : math::Vec2{};
        SpriteInstance instance = placeGlyph(glyph, {}, color, flash);
        instance.position += moved;
        add(program, page, GpuInstance::makeGlyph(*page.getResource(), instance, parameters));
        if (look.emboldenOffset > 0.0F) {
            SpriteInstance copy = placeGlyph(glyph, {look.emboldenOffset, 0.0F}, color, flash);
            copy.position += moved;
            add(program, page, GpuInstance::makeGlyph(*page.getResource(), copy, parameters));
        }
    }
}

math::Vec2 TextPainter::place(math::Vec2 local) const noexcept {
    const math::Vec2 scaled = (local - blockOrigin) * blockScale;
    if (blockRotation == 0.0F) {
        return blockPosition + scaled;
    }
    const float cosine = std::cos(blockRotation);
    const float sine = std::sin(blockRotation);
    return blockPosition + math::Vec2{scaled.x * cosine - scaled.y * sine, scaled.x * sine + scaled.y * cosine};
}

SpriteInstance TextPainter::placeGlyph(const text::TextLayout::Glyph& glyph, math::Vec2 offset, math::Color color, math::Color flash) const noexcept {
    const float above = glyph.baseline - glyph.position.y;
    return {
        .position = place(math::Vec2{glyph.position.x, glyph.baseline} + offset),
        .size = glyph.size * blockScale,
        .source = glyph.source,
        .pivot = {0.0F, glyph.size.y > 0.0F ? above / glyph.size.y : 0.0F},
        .rotation = blockRotation,
        .color = color * blockTint,
        .flash = flash * blockTint,
    };
}

// Rich text draws in layers: backgrounds, glows, shadows, images, glyphs, and then the lines over the text.
void TextPainter::paintRichText(const text::TextLayout& layout, math::Vec2 position, math::Vec2 scale, math::Color tint) {
    blockPosition = position;
    blockOrigin = {};
    blockScale = scale;
    blockRotation = 0.0F;
    blockTint = tint;
    paintBoxes(layout, true);
    paintGlows(layout);
    paintShadows(layout);
    for (const text::TextLayout::Image& image : layout.images) {
        if (image.visible) {
            add(Program::Sprite, image.texture, GpuInstance::make(*image.texture.getResource(), {.position = place(image.rect.getMin()), .size = image.rect.getSize() * scale, .source = image.source, .pivot = {}, .color = image.color * tint}));
        }
    }
    paintGlyphs(layout);
    paintBoxes(layout, false);
}

void TextPainter::paintBoxes(const text::TextLayout& layout, bool underText) {
    using Kind = text::TextLayout::Box::Kind;
    for (const text::TextLayout::Box& box : layout.boxes) {
        const bool under = box.kind == Kind::Background || box.kind == Kind::CellBackground;
        if (box.visible && under == underText && !box.rect.isEmpty()) {
            add(Program::Sprite, white, GpuInstance::make(*white.getResource(), {.position = place(box.rect.getMin()), .size = box.rect.getSize() * blockScale, .pivot = {}, .color = box.color * blockTint}));
        }
    }
}

// A glow is the glyph grown past its outline by half the glow width and softened over the other half, so it fades out a glow width away from the edge. Only distance field fonts glow.
void TextPainter::paintGlows(const text::TextLayout& layout) {
    for (const text::TextLayout::Glyph& glyph : layout.glyphs) {
        const text::TextLayout::Look& look = layout.looks[glyph.look];
        if (!glyph.visible || !look.glow || !look.font->isDistanceField()) {
            continue;
        }
        const float half = look.glow->width * 0.5F;
        const GpuInstance::TextParameters glow{.weight = look.font->toDistance(look.weight + look.outlineWidth + half, look.size), .skew = look.skew, .softness = look.font->toDistance(half, look.size)};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        const math::Color color = look.glow->color.withAlpha(look.glow->color.a * glyph.color.a);
        add(Program::Text, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {}, color, math::Color::transparent()), glow));
    }
}

void TextPainter::paintShadows(const text::TextLayout& layout) {
    for (const text::TextLayout::Glyph& glyph : layout.glyphs) {
        const text::TextLayout::Look& look = layout.looks[glyph.look];
        if (!glyph.visible || !look.shadow) {
            continue;
        }
        const bool field = look.font->isDistanceField();
        const math::Color color = look.shadow->color.withAlpha(look.shadow->color.a * glyph.color.a);
        const float outline = look.font->toDistance(look.outlineWidth, look.size);
        const math::Color flash = field ? color.withAlpha(outline > 0.0F ? color.a : 0.0F) : color.withAlpha(1.0F);
        const GpuInstance::TextParameters shadow{.outline = outline, .weight = look.font->toDistance(look.weight, look.size), .skew = look.skew, .softness = look.font->toDistance(look.shadow->blur, look.size)};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        add(field ? Program::Text : Program::Sprite, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, look.shadow->offset, color, flash), shadow));
    }
}

// A synthetic bold bitmap glyph draws a second copy a native pixel to the right.
void TextPainter::paintGlyphs(const text::TextLayout& layout) {
    for (const text::TextLayout::Glyph& glyph : layout.glyphs) {
        if (!glyph.visible) {
            continue;
        }
        const text::TextLayout::Look& look = layout.looks[glyph.look];
        const bool field = look.font->isDistanceField();
        const float outline = look.font->toDistance(look.outlineWidth, look.size);
        const math::Color flash = field && outline > 0.0F ? look.outlineColor.withAlpha(look.outlineColor.a * glyph.color.a) : math::Color::transparent();
        const GpuInstance::TextParameters parameters{.outline = outline, .weight = look.font->toDistance(look.weight, look.size), .skew = look.skew};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        const Program program = field ? Program::Text : Program::Sprite;
        add(program, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {}, glyph.color, flash), parameters));
        if (look.emboldenOffset > 0.0F) {
            add(program, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {look.emboldenOffset, 0.0F}, glyph.color, flash), parameters));
        }
    }
}

} // namespace haylen::graphics2d
