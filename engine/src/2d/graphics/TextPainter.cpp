#include "2d/graphics/TextPainter.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "graphics/TextureResource.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::graphics2d {

TextPainter::TextPainter(graphics::Texture whiteTexture, VectorAtlas& vectorAtlas) : white(std::move(whiteTexture)), vectors(vectorAtlas) {}

float TextPainter::PixelGrid::snapX(float x) const noexcept {
    return (std::round(x * scale.x + offset.x) - offset.x) / scale.x;
}

float TextPainter::PixelGrid::snapY(float y) const noexcept {
    return (std::round(y * scale.y + offset.y) - offset.y) / scale.y;
}

void TextPainter::clear() noexcept {
    for (std::size_t index = 0; index < batchCount; ++index) {
        batches[index].texture = {};
        batches[index].instances.clear();
    }
    batchCount = 0;
}

void TextPainter::add(Program program, const graphics::Texture& texture, const GpuInstance& instance) {
    if (batchCount == 0 || batches[batchCount - 1].program != program || batches[batchCount - 1].texture != texture) {
        if (batchCount == batches.size()) {
            batches.emplace_back();
        }
        Batch& next = batches[batchCount++];
        next.program = program;
        next.texture = texture;
        next.instances.clear();
    }
    batches[batchCount - 1].instances.push_back(instance);
}

// A glyph with a gradient takes the record of its gradient right after its own.
void TextPainter::addGradientGlyph(const graphics::Texture& page, const GpuInstance& glyph, const GpuInstance& gradient) {
    add(Program::GradientText, page, glyph);
    batches[batchCount - 1].instances.push_back(gradient);
}

// A gradient runs across the box of the line of the glyph, or across the block for a glyph outside every line, and reaches 0 and 1 at the corners furthest along its direction. The position along it is linear in the point of the layout, so it is linear in the point of the canvas too, through the inverse of the scale and the rotation that place the block.
GpuInstance TextPainter::makeGradient(const text::Layout& layout, const text::Layout::Glyph& glyph, const text::Gradient& gradient) const noexcept {
    const auto line = std::ranges::upper_bound(layout.lines, glyph.character, {}, &text::Layout::Line::endCharacter);
    const math::Rect area = line != layout.lines.end() ? line->box : math::Rect{0.0F, 0.0F, layout.size.x, layout.size.y};
    const math::Vec2 direction{std::cos(gradient.getAngle()), std::sin(gradient.getAngle())};
    const float extent = std::fabs(area.width * direction.x) + std::fabs(area.height * direction.y);
    const math::Vec2 slope = extent > 0.0F ? direction / extent : math::Vec2{};

    const math::Vec2 unscaled{blockScale.x != 0.0F ? slope.x / blockScale.x : 0.0F, blockScale.y != 0.0F ? slope.y / blockScale.y : 0.0F};
    const float cosine = std::cos(blockRotation);
    const float sine = std::sin(blockRotation);
    const math::Vec2 axis{unscaled.x * cosine - unscaled.y * sine, unscaled.x * sine + unscaled.y * cosine};
    const float base = 0.5F + math::Vec2::dot(blockOrigin - area.getCenter(), slope) - math::Vec2::dot(blockPosition, axis);
    return GpuInstance::makeGradient(gradient, axis, base);
}

// A color glyph draws the raster of its vector image that suits the pixels its quad covers, through the sprite program.
void TextPainter::addColorGlyph(const text::Layout::Glyph& glyph, SpriteInstance instance) {
    const math::Vec2 natural = glyph.image.getSize();
    const float scale = std::max(std::fabs(instance.size.x) / natural.x, std::fabs(instance.size.y) / natural.y) * target.pixelsPerUnit;
    if (!(scale > 0.0F && std::isfinite(scale))) {
        return;
    }
    const VectorAtlas::Raster raster = vectors.find(glyph.image, scale, target.frame);
    instance.source = raster.source;
    add(Program::Sprite, raster.texture, GpuInstance::make(*raster.texture.getResource(), instance));
}

// Plain text turns as one piece around the position, so every glyph turns around its baseline at a turned place. It draws in layers: shadows, the same glyphs moved by the offset on the screen in the shadow color with the blur as softness, where the shadow of a bitmap or color glyph is its silhouette, then the outlines of every glyph, and the fills over them, so no outline covers a neighbouring letter.
void TextPainter::paintText(const text::Layout& layout, math::Vec2 position, const text::Style& style, const Target& canvas, const std::optional<PixelGrid>& grid) {
    clear();
    target = canvas;
    blockPosition = position;
    blockOrigin = layout.size * style.anchor;
    blockScale = style.scale;
    blockRotation = style.rotation;
    blockTint = math::Color::white();
    pixels = style.rotation == 0.0F ? grid : std::nullopt;
    if (style.shadowColor.a > 0.0F) {
        paintPlainGlyphs(layout, style, Layer::Shadow);
    }
    if (style.outlineWidth > 0.0F) {
        paintPlainGlyphs(layout, style, Layer::Outline);
    }
    paintPlainGlyphs(layout, style, Layer::Fill);
    paintBoxes(layout, false, style.color);
}

// A synthetic bold bitmap glyph draws a second copy a native pixel to the right. Only distance field glyphs have outlines, and color glyphs fill with their own colors in the alpha of the text.
void TextPainter::paintPlainGlyphs(const text::Layout& layout, const text::Style& style, Layer layer) {
    std::size_t line = 0;
    for (const text::Layout::Glyph& glyph : layout.glyphs) {
        // Every line lands on a pixel column of its own, since a centered or right-aligned line may start half a pixel from the others.
        if (pixels) {
            while (line + 1 < layout.lines.size() && glyph.character >= layout.lines[line].endCharacter) {
                ++line;
            }
            const float left = place({layout.lines[line].box.x, 0.0F}).x;
            lineShift = pixels->snapX(left) - left;
        }
        const text::Layout::Look& look = layout.looks[glyph.look];
        const bool colored = glyph.image.isValid();
        const bool field = look.font->isDistanceField() && !colored;
        const float outline = look.font->toDistance(style.outlineWidth, look.size);
        if (layer == Layer::Outline && !(field && outline > 0.0F)) {
            continue;
        }

        const math::Vec2 moved = layer == Layer::Shadow ? style.shadowOffset : math::Vec2{};
        if (colored) {
            const bool shadow = layer == Layer::Shadow;
            const math::Color color = shadow ? style.shadowColor : math::Color::white().withAlpha(style.color.a);
            SpriteInstance instance = placeGlyph(glyph, {}, color, shadow ? color.withAlpha(1.0F) : math::Color::transparent());
            instance.position += moved;
            addColorGlyph(glyph, instance);
            continue;
        }

        math::Color color = style.color;
        math::Color flash = math::Color::transparent();
        GpuInstance::TextParameters parameters{.weight = look.font->toDistance(look.weight, look.size), .width = look.font->toDistance(glyph.size.x, look.size), .skew = look.skew};
        if (layer == Layer::Shadow) {
            color = style.shadowColor;
            flash = field ? color.withAlpha(outline > 0.0F ? color.a : 0.0F) : color.withAlpha(1.0F);
            parameters.outline = outline;
            parameters.softness = look.font->toDistance(style.shadowBlur, look.size);
        } else if (layer == Layer::Outline) {
            color = math::Color::transparent();
            flash = style.outlineColor;
            parameters.outline = outline;
        }

        const graphics::Texture& page = look.font->getPage(glyph.page);
        SpriteInstance instance = placeGlyph(glyph, {}, color, flash);
        instance.position += moved;
        if (layer == Layer::Fill && field && style.gradient) {
            const GpuInstance gradient = makeGradient(layout, glyph, *style.gradient);
            addGradientGlyph(page, GpuInstance::makeGlyph(*page.getResource(), instance, parameters), gradient);
            continue;
        }

        const Program program = field ? Program::Text : Program::Sprite;
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

SpriteInstance TextPainter::placeGlyph(const text::Layout::Glyph& glyph, math::Vec2 offset, math::Color color, math::Color flash) const noexcept {
    const float above = glyph.baseline - glyph.position.y;
    math::Vec2 pen = place(math::Vec2{glyph.position.x, glyph.baseline} + offset);
    if (pixels) {
        pen.x += lineShift;
        pen.y = pixels->snapY(pen.y);
    }
    return {
        .position = pen,
        .size = glyph.size * blockScale,
        .source = glyph.source,
        .pivot = {0.0F, glyph.size.y > 0.0F ? above / glyph.size.y : 0.0F},
        .rotation = blockRotation,
        .color = color * blockTint,
        .flash = flash * blockTint,
    };
}

// Rich text draws in layers: backgrounds, glows, shadows, images, outlines, glyphs, and then the lines over the text.
void TextPainter::paintRichText(const text::Layout& layout, math::Vec2 position, const Target& canvas, math::Vec2 scale, math::Color tint) {
    clear();
    target = canvas;
    blockPosition = position;
    blockOrigin = {};
    blockScale = scale;
    blockRotation = 0.0F;
    blockTint = tint;
    pixels.reset();
    paintBoxes(layout, true, blockTint);
    paintGlows(layout);
    paintShadows(layout);
    for (const text::Layout::Image& image : layout.images) {
        if (image.visible) {
            add(Program::Sprite, image.texture, GpuInstance::make(*image.texture.getResource(), {.position = place(image.rect.getMin()), .size = image.rect.getSize() * scale, .source = image.source, .pivot = {}, .color = image.color * tint}));
        }
    }
    paintOutlines(layout);
    paintGlyphs(layout);
    paintBoxes(layout, false, blockTint);
}

void TextPainter::paintBoxes(const text::Layout& layout, bool underText, math::Color tint) {
    using Kind = text::Layout::Box::Kind;
    for (const text::Layout::Box& box : layout.boxes) {
        const bool under = box.kind == Kind::Background || box.kind == Kind::CellBackground;
        if (box.visible && under == underText && !box.rect.isEmpty()) {
            add(Program::Sprite, white, GpuInstance::make(*white.getResource(), {.position = place(box.rect.getMin()), .size = box.rect.getSize() * blockScale, .pivot = {}, .rotation = blockRotation, .color = box.color * tint}));
        }
    }
}

// A glow is the outline of the glyph grown by half the glow width and softened over the other half, so it fades out a glow width away from the edge of the outline. Only the glyphs of distance fields glow.
void TextPainter::paintGlows(const text::Layout& layout) {
    for (const text::Layout::Glyph& glyph : layout.glyphs) {
        const text::Layout::Look& look = layout.looks[glyph.look];
        if (!glyph.visible || !look.glow || !look.font->isDistanceField() || glyph.image.isValid()) {
            continue;
        }
        const float half = look.glow->width * 0.5F;
        const GpuInstance::TextParameters glow{.outline = look.font->toDistance(look.outlineWidth + half, look.size), .weight = look.font->toDistance(look.weight, look.size), .width = look.font->toDistance(glyph.size.x, look.size), .skew = look.skew, .softness = look.font->toDistance(half, look.size)};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        const math::Color color = look.glow->color.withAlpha(look.glow->color.a * glyph.color.a);
        add(Program::Text, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {}, math::Color::transparent(), color), glow));
    }
}

void TextPainter::paintShadows(const text::Layout& layout) {
    for (const text::Layout::Glyph& glyph : layout.glyphs) {
        const text::Layout::Look& look = layout.looks[glyph.look];
        if (!glyph.visible || !look.shadow) {
            continue;
        }
        const math::Color color = look.shadow->color.withAlpha(look.shadow->color.a * glyph.color.a);
        if (glyph.image.isValid()) {
            addColorGlyph(glyph, placeGlyph(glyph, look.shadow->offset, color, color.withAlpha(1.0F)));
            continue;
        }
        const bool field = look.font->isDistanceField();
        const float outline = look.font->toDistance(look.outlineWidth, look.size);
        const math::Color flash = field ? color.withAlpha(outline > 0.0F ? color.a : 0.0F) : color.withAlpha(1.0F);
        const GpuInstance::TextParameters shadow{.outline = outline, .weight = look.font->toDistance(look.weight, look.size), .width = look.font->toDistance(glyph.size.x, look.size), .skew = look.skew, .softness = look.font->toDistance(look.shadow->blur, look.size)};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        add(field ? Program::Text : Program::Sprite, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, look.shadow->offset, color, flash), shadow));
    }
}

// Every outline lies under every fill, so the outline of a letter never covers the letters next to it.
void TextPainter::paintOutlines(const text::Layout& layout) {
    for (const text::Layout::Glyph& glyph : layout.glyphs) {
        const text::Layout::Look& look = layout.looks[glyph.look];
        const float outline = look.font->toDistance(look.outlineWidth, look.size);
        if (!glyph.visible || !look.font->isDistanceField() || glyph.image.isValid() || outline <= 0.0F) {
            continue;
        }
        const math::Color flash = look.outlineColor.withAlpha(look.outlineColor.a * glyph.color.a);
        const GpuInstance::TextParameters parameters{.outline = outline, .weight = look.font->toDistance(look.weight, look.size), .width = look.font->toDistance(glyph.size.x, look.size), .skew = look.skew};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        add(Program::Text, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {}, math::Color::transparent(), flash), parameters));
    }
}

// A synthetic bold bitmap glyph draws a second copy a native pixel to the right, and a color glyph draws in its own colors with the alpha of its color.
void TextPainter::paintGlyphs(const text::Layout& layout) {
    for (const text::Layout::Glyph& glyph : layout.glyphs) {
        if (!glyph.visible) {
            continue;
        }
        if (glyph.image.isValid()) {
            addColorGlyph(glyph, placeGlyph(glyph, {}, math::Color::white().withAlpha(glyph.color.a), math::Color::transparent()));
            continue;
        }
        const text::Layout::Look& look = layout.looks[glyph.look];
        const GpuInstance::TextParameters parameters{.weight = look.font->toDistance(look.weight, look.size), .width = look.font->toDistance(glyph.size.x, look.size), .skew = look.skew};
        const graphics::Texture& page = look.font->getPage(glyph.page);
        const GpuInstance placed = GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {}, glyph.color, math::Color::transparent()), parameters);
        if (look.gradient && look.font->isDistanceField()) {
            addGradientGlyph(page, placed, makeGradient(layout, glyph, *look.gradient));
            continue;
        }

        const Program program = look.font->isDistanceField() ? Program::Text : Program::Sprite;
        add(program, page, placed);
        if (look.emboldenOffset > 0.0F) {
            add(program, page, GpuInstance::makeGlyph(*page.getResource(), placeGlyph(glyph, {look.emboldenOffset, 0.0F}, glyph.color, math::Color::transparent()), parameters));
        }
    }
}

} // namespace haylen::graphics2d
