#include "ui/components/text/SectionTitle.hpp"

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void SectionTitle::readProperties(PropertyReader& reader) {
    reader.read("text", text);
}

math::Vec2 SectionTitle::measureContent(Context& context, float availableWidth) {
    const math::Vec2 size = Typography::measureParagraph(context, Theme::Font::Heading, context.getText(text), availableWidth);
    return {size.x, size.y + context.getMetric(Theme::Metric::ItemSpacing) * 0.5F};
}

void SectionTitle::render(Context& context, const math::Rect& bounds) {
    Typography::drawParagraph(context, Theme::Font::Heading, bounds, context.getColor(Theme::Color::Text), context.getText(text), Alignment::Start);
}

} // namespace haylen::ui
