#include "ui/components/text/Label.hpp"

#include <string>

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

void Label::render(Context& context, const math::Rect& bounds) {
    const std::string shown = context.getText(text);
    if (wrap) {
        Typography::drawParagraph(context, font, bounds, context.getColor(color.value_or(Theme::Color::Text)), shown, textAlign, outline, outlineWidth);
        return;
    }
    const float height = Typography::getLineHeight(context, font);
    const math::Rect line{bounds.x, bounds.y + (bounds.height - height) * 0.5F, bounds.width, height};
    Typography::drawParagraph(context, font, line, context.getColor(color.value_or(Theme::Color::Text)), Typography::elide(context, font, shown, bounds.width), textAlign, outline, outlineWidth);
}

} // namespace haylen::ui
