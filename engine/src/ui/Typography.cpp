#include "ui/Typography.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

float Typography::getLineHeight(Context& context, Theme::Font font) {
    return context.getFontSize(font);
}

math::Vec2 Typography::measure(Context& context, Theme::Font font, std::string_view text, float wrapWidth) {
    const ImVec2 size = context.getFont(font)->CalcTextSizeA(context.getFontSize(font), FLT_MAX, wrapWidth > 0.0F ? wrapWidth : 0.0F, text.data(), text.data() + text.size());
    return {size.x, text.empty() ? getLineHeight(context, font) : size.y};
}

void Typography::draw(Context& context, Theme::Font font, math::Vec2 position, math::Color color, std::string_view text, float wrapWidth) {
    ImGui::GetWindowDrawList()->AddText(context.getFont(font), context.getFontSize(font), ImGuiConverter::toImVec2(position), ImGuiConverter::toImU32(color), text.data(), text.data() + text.size(), wrapWidth > 0.0F ? wrapWidth : 0.0F);
}

std::string Typography::elide(Context& context, Theme::Font font, std::string_view text, float width) {
    if (measure(context, font, text).x <= width) {
        return std::string(text);
    }

    // The longest prefix that still fits with the ellipsis wins, cut only between whole UTF-8 characters.
    std::size_t low = 0;
    std::size_t high = text.size();
    while (low < high) {
        std::size_t middle = (low + high + 1) / 2;
        while (middle > 0 && middle < text.size() && (static_cast<unsigned char>(text[middle]) & 0xC0U) == 0x80U) {
            --middle;
        }
        if (middle <= low) {
            break;
        }
        const std::string candidate = std::string(text.substr(0, middle)) + std::string(kEllipsis);
        if (measure(context, font, candidate).x <= width) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return std::string(text.substr(0, low)) + std::string(kEllipsis);
}

std::vector<std::string_view> Typography::wrapLines(Context& context, Theme::Font font, std::string_view text, float width) {
    std::vector<std::string_view> lines;
    ImFont* face = context.getFont(font);
    const float size = context.getFontSize(font);
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find('\n', start);
        const std::string_view line = text.substr(start, newline == std::string_view::npos ? std::string_view::npos : newline - start);
        if (width <= 0.0F || line.empty()) {
            lines.push_back(line);
        } else {
            const char* cursor = line.data();
            const char* end = line.data() + line.size();
            while (cursor < end) {
                const char* wrap = face->CalcWordWrapPosition(size, cursor, end, width + kRoundingSlack);
                if (wrap == cursor) {
                    wrap = cursor + 1;
                    while (wrap < end && (static_cast<unsigned char>(*wrap) & 0xC0U) == 0x80U) {
                        ++wrap;
                    }
                }
                std::string_view piece(cursor, static_cast<std::size_t>(wrap - cursor));
                while (!piece.empty() && piece.back() == ' ') {
                    piece.remove_suffix(1);
                }
                lines.push_back(piece);
                cursor = wrap;
                while (cursor < end && *cursor == ' ') {
                    ++cursor;
                }
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }
    return lines;
}

math::Vec2 Typography::measureParagraph(Context& context, Theme::Font font, std::string_view text, float width) {
    const std::vector<std::string_view> lines = wrapLines(context, font, text, width);
    float widest = 0.0F;
    for (const std::string_view line : lines) {
        widest = std::max(widest, measure(context, font, line).x);
    }
    return {widest, getLineHeight(context, font) * static_cast<float>(lines.size())};
}

void Typography::drawParagraph(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal, std::optional<math::Color> outline, float outlineWidth) {
    const float height = getLineHeight(context, font);
    const float factor = horizontal == Alignment::Center ? 0.5F : (horizontal == Alignment::End ? 1.0F : 0.0F);
    float y = bounds.y;
    for (const std::string_view line : wrapLines(context, font, text, bounds.width)) {
        const math::Vec2 position{std::floor(bounds.x + (bounds.width - measure(context, font, line).x) * factor), std::floor(y)};
        if (outline) {
            for (const math::Vec2 offset : {math::Vec2{-1.0F, 0.0F}, math::Vec2{1.0F, 0.0F}, math::Vec2{0.0F, -1.0F}, math::Vec2{0.0F, 1.0F}, math::Vec2{-1.0F, -1.0F}, math::Vec2{1.0F, 1.0F}, math::Vec2{-1.0F, 1.0F}, math::Vec2{1.0F, -1.0F}}) {
                draw(context, font, position + offset * outlineWidth, *outline, line);
            }
        }
        draw(context, font, position, color, line);
        y += height;
    }
}

void Typography::drawAligned(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal) {
    const std::string shown = elide(context, font, text, bounds.width);
    const float width = measure(context, font, shown).x;
    const float factor = horizontal == Alignment::Center ? 0.5F : (horizontal == Alignment::End ? 1.0F : 0.0F);
    const math::Vec2 position{std::floor(bounds.x + (bounds.width - width) * factor), std::floor(bounds.y + (bounds.height - getLineHeight(context, font)) * 0.5F)};
    draw(context, font, position, color, shown);
}

std::string Typography::formatNumber(double value, int decimals) {
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), "%.*f", decimals, value);
    return text.data();
}

} // namespace haylen::ui
