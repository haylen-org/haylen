#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/TextLayout.hpp"
#include "haylen/text/TextStyle.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// Measures, wraps, shortens and draws text in the font families of the theme, so every component sets text the same way. Text is shaped and ordered for display by the text layout of the engine in the language of the node, where every paragraph reads in the direction of its first strong letter and lines up with the side of the UI its alignment names. Layouts are cached by text and style, and text draws through the 2D renderer at its place among the ImGui draws, inside their clip.
class Typography final {
  public:
    // The alignments a text property may name.
    static constexpr std::array<std::pair<std::string_view, Alignment>, 3> kAlignments{{
        {"start", Alignment::Start},
        {"center", Alignment::Center},
        {"end", Alignment::End},
    }};

    [[nodiscard]] static float getLineHeight(Context& context, Theme::Font font);
    [[nodiscard]] static math::Vec2 measure(Context& context, Theme::Font font, std::string_view text, float wrapWidth = -1.0F);

    // Draws text with the top of its block at the position, whose lines start from the left or, in a right-to-left UI, end at the wrap width.
    static void draw(Context& context, Theme::Font font, math::Vec2 position, math::Color color, std::string_view text, float wrapWidth = -1.0F);

    // Draws one line centered vertically in the bounds, shortened with an ellipsis when it does not fit.
    static void drawAligned(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal);
    [[nodiscard]] static std::string elide(Context& context, Theme::Font font, std::string_view text, float width);

    [[nodiscard]] static math::Vec2 measureParagraph(Context& context, Theme::Font font, std::string_view text, float width);

    // Draws wrapped lines aligned inside the bounds, where start and end follow the direction of the UI. An outline keeps light text readable over busy scenes.
    static void drawParagraph(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal, std::optional<math::Color> outline = std::nullopt, float outlineWidth = 2.0F);

    // Returns the style that lays text out in a font role of the theme in the language of the node being drawn, where start and end become the sides of the UI they name.
    [[nodiscard]] static text::TextStyle getStyle(Context& context, Theme::Font font, float wrapWidth = -1.0F, text::TextAlign align = text::TextAlign::Start);
    [[nodiscard]] static std::shared_ptr<const text::TextLayout> layout(Context& context, Theme::Font font, std::string_view text, const text::TextStyle& style);

    // Draws a layout of the text in the style at the position of the top-left of its block, moved, scaled and tinted by the transforms of the nodes around it.
    static void drawLayout(Context& context, Theme::Font font, std::string_view text, const text::TextStyle& style, math::Vec2 position);

    // Formats a number with a fixed count of decimals.
    [[nodiscard]] static std::string formatNumber(double value, int decimals);

  private:
    static constexpr std::string_view kEllipsis = "\xE2\x80\xA6";
};

} // namespace haylen::ui
