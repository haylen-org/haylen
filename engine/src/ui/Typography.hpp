#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// Measures, wraps, shortens and draws text in the fonts of the theme, so every component sets text the same way.
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
    static void draw(Context& context, Theme::Font font, math::Vec2 position, math::Color color, std::string_view text, float wrapWidth = -1.0F);

    // Draws one line centered vertically in the bounds, shortened with an ellipsis when it does not fit.
    static void drawAligned(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal);
    [[nodiscard]] static std::string elide(Context& context, Theme::Font font, std::string_view text, float width);

    // Splits text at its line breaks and, when the width is positive, wherever a line would grow wider than it.
    [[nodiscard]] static std::vector<std::string_view> wrapLines(Context& context, Theme::Font font, std::string_view text, float width);
    [[nodiscard]] static math::Vec2 measureParagraph(Context& context, Theme::Font font, std::string_view text, float width);

    // Draws wrapped lines aligned inside the bounds. An outline keeps light text readable over busy scenes.
    static void drawParagraph(Context& context, Theme::Font font, const math::Rect& bounds, math::Color color, std::string_view text, Alignment horizontal, std::optional<math::Color> outline = std::nullopt, float outlineWidth = 2.0F);

    // Formats a number with a fixed count of decimals.
    [[nodiscard]] static std::string formatNumber(double value, int decimals);

  private:
    static constexpr std::string_view kEllipsis = "\xE2\x80\xA6";

    // ImGui wraps with unscaled advances and measures with scaled ones, so a line measured at a width can overflow it by a rounding error.
    static constexpr float kRoundingSlack = 1.0F;
};

} // namespace haylen::ui
