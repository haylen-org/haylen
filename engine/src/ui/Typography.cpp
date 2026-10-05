#include "ui/Typography.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <imgui.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"

namespace haylen::ui {

float Typography::getLineHeight(Context& context, Theme::Font font) {
    return context.getFontSize(font);
}

// Text takes the em size that lines its letters up with the widgets of its role, since the theme sizes a font by its height from ascent to descent, and lines follow each other by the height of their fonts. Every paragraph reads in the direction of its first strong letter, while start and end line it up with the side of the UI they name.
text::Style Typography::getStyle(Context& context, Theme::Font font, float wrapWidth, text::Alignment align) {
    const Theme::FontStyle& role = context.getTheme().getFont(font);
    const bool rightToLeft = context.isRightToLeft();
    if (align == text::Alignment::Start || align == text::Alignment::End) {
        align = (align == text::Alignment::Start) != rightToLeft ? text::Alignment::Left : text::Alignment::Right;
    }
    return {.size = context.getEmSize(font), .align = align, .maxWidth = wrapWidth > 0.0F ? wrapWidth : 0.0F, .lineSpacing = 1.0F, .bold = role.bold, .italic = role.italic, .direction = text::Direction::Auto, .language = context.getLanguage()};
}

std::shared_ptr<const text::Layout> Typography::layout(Context& context, Theme::Font font, std::string_view text, const text::Style& style) {
    return context.getFontFamily(font)->layout(text, style);
}

// Widths round up to whole design units, so the sizes a layout adds up and takes apart again stay exact and text drawn at its measured width never wraps. A wrapped paragraph is as wide as its widest line.
math::Vec2 Typography::measure(Context& context, Theme::Font font, std::string_view text, float wrapWidth) {
    if (text.empty()) {
        return {0.0F, getLineHeight(context, font)};
    }
    const std::shared_ptr<const text::Layout> laid = layout(context, font, text, getStyle(context, font, wrapWidth));
    float width = 0.0F;
    for (const text::Layout::Line& line : laid->lines) {
        width = std::max(width, line.box.width);
    }
    return {std::ceil(width), laid->size.y};
}

// The text draws through the renderer once the frame renders, at its place among the ImGui draws of the window and inside their clip.
void Typography::drawLayout(Context& context, Theme::Font font, std::string_view text, const text::Style& style, math::Vec2 position) {
    if (text.empty()) {
        return;
    }
    // The text fades with the transforms around it and with the alpha of ImGui, which disabled nodes and fading overlays lower.
    const Context::Reshape shape = context.getReshape();
    const math::Color faded = shape.color.withAlpha(shape.color.a * ImGui::GetStyle().Alpha);
    text::Style painted = style;
    painted.color = style.color * faded;
    painted.outlineColor = style.outlineColor * faded;
    painted.scale = shape.scale;
    const math::Vec2 origin = math::Vec2{std::floor(position.x), std::floor(position.y)} * shape.scale + shape.offset;
    // clang-format off
    context.getBackend().addRenderCallback([family = context.getFontFamily(font), content = std::string(text), painted, origin](graphics2d::Renderer& renderer, math::Vec2 offset) {
        renderer.drawText(*family, content, origin + offset, painted);
    });
    // clang-format on
}

// The longest run of whole characters in reading order that fits with the ellipsis stays, and since shaping may change the width where the text is cut, the run shortens until the result fits.
std::string Typography::elide(Context& context, Theme::Font font, std::string_view text, float width) {
    const text::Style style = getStyle(context, font);
    const std::shared_ptr<const text::Layout> full = layout(context, font, text, style);
    if (full->size.x <= width) {
        return std::string(text);
    }

    const float ellipsis = layout(context, font, kEllipsis, style)->size.x;
    std::size_t kept = 0;
    float used = 0.0F;
    while (kept < full->characters.size() && used + full->characters[kept].box.width + ellipsis <= width) {
        used += full->characters[kept].box.width;
        ++kept;
    }
    while (true) {
        const std::size_t cut = kept < full->characters.size() ? full->characters[kept].begin : core::Utf8::countCodePoints(text);
        std::string shortened = std::string(text.substr(0, core::Utf8::getOffset(text, cut))) + std::string(kEllipsis);
        if (kept == 0 || layout(context, font, shortened, style)->size.x <= width) {
            return shortened;
        }
        --kept;
    }
}

math::Vec2 Typography::measureParagraph(Context& context, Theme::Font font, std::string_view text, float width) {
    return measure(context, font, text, width);
}

void Typography::drawParagraph(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal, std::optional<math::Color> outline, float outlineWidth) {
    const text::Alignment align = horizontal == Alignment::Center ? text::Alignment::Center : (horizontal == Alignment::End ? text::Alignment::End : text::Alignment::Start);
    text::Style style = getStyle(context, font, bounds.width, align);
    style.color = color;
    if (outline) {
        style.outlineWidth = outlineWidth;
        style.outlineColor = *outline;
    }
    drawLayout(context, font, text, style, bounds.getMin());
}

void Typography::drawAligned(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal) {
    const std::string shown = elide(context, font, text, bounds.width);
    text::Style style = getStyle(context, font);
    style.color = color;
    const math::Vec2 size = layout(context, font, shown, style)->size;
    drawLayout(context, font, shown, style, {context.alignHorizontally(horizontal, bounds.x, bounds.width, size.x), bounds.y + (bounds.height - size.y) * 0.5F});
}

std::string Typography::formatNumber(double value, int decimals) {
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), "%.*f", decimals, value);
    return text.data();
}

} // namespace haylen::ui
