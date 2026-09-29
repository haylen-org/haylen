#include "ui/components/indicators/Progress.hpp"

#include <algorithm>
#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Progress::readProperties(PropertyReader& reader) {
    reader.read("value", value, 0.0F, 1.0F);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("text", text);
}

math::Vec2 Progress::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 320.0F), std::max(context.getMetric(Theme::Metric::ProgressHeight), text.isEmpty() ? 0.0F : Typography::getLineHeight(context, Theme::Font::Caption) + 8.0F)};
}

void Progress::render(Context& context, const math::Rect& bounds) {
    Widgets::progress(context, bounds, value, tone);
    if (const std::string shown = context.getText(text); !shown.empty()) {
        Typography::drawParagraph(context, Theme::Font::Caption, {bounds.x, bounds.getCenter().y - Typography::getLineHeight(context, Theme::Font::Caption) * 0.5F, bounds.width, Typography::getLineHeight(context, Theme::Font::Caption)}, context.getColor(Theme::Color::Text), shown, Alignment::Center, context.getColor(Theme::Color::Window), 1.0F);
    }
}

} // namespace haylen::ui
