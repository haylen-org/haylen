#include "ui/components/text/Label.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void Label::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.readChoice<Theme::Font>("font", font, kFonts);
    reader.read("color", color);
    reader.readChoice<Alignment>("textAlign", textAlign, Typography::kAlignments);
    reader.read("wrap", wrap);
    if (reader.has("outline")) {
        math::Color outlineColor;
        reader.read("outline", outlineColor);
        outline = outlineColor;
    }
    reader.read("outlineWidth", outlineWidth, 0.0F, 16.0F);
}

math::Vec2 Label::measureContent(Context& context, float availableWidth) {
    return Typography::measureParagraph(context, font, context.getText(text), wrap ? availableWidth : -1.0F);
}

// Without wrapping every line of the text keeps its own line and ends with an ellipsis when it does not fit, and the block of lines sits in the middle of the height.
void Label::render(Context& context, const math::Rect& bounds) {
    const std::string full = context.getText(text);
    if (wrap) {
        Typography::drawParagraph(context, font, bounds, context.getColor(color.value_or(Theme::Color::Text)), full, textAlign, outline, outlineWidth);
        return;
    }
    std::string shown;
    std::size_t start = 0;
    while (start <= full.size()) {
        const std::size_t end = std::min(full.find('\n', start), full.size());
        shown += Typography::elide(context, font, std::string_view(full).substr(start, end - start), bounds.width);
        if (end < full.size()) {
            shown += '\n';
        }
        start = end + 1;
    }
    const float height = Typography::measureParagraph(context, font, shown, -1.0F).y;
    const math::Rect block{bounds.x, std::floor(bounds.y + (bounds.height - height) * 0.5F), bounds.width, height};
    Typography::drawParagraph(context, font, block, context.getColor(color.value_or(Theme::Color::Text)), shown, textAlign, outline, outlineWidth);
}

} // namespace haylen::ui
