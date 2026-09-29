#include "ui/components/containers/FormField.hpp"

#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void FormField::readProperties(PropertyReader& reader) {
    reader.read("label", label);
    reader.read("help", help);
    reader.read("error", error);
    reader.read("required", required);
}

math::Vec2 FormField::measureContent(Context& context, float availableWidth) {
    math::Vec2 size;
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing) * 0.5F;
    if (!label.isEmpty()) {
        size.y += Typography::getLineHeight(context, Theme::Font::Caption) + spacing;
    }
    if (!getChildren().empty()) {
        const math::Vec2 control = getChildren().front()->measure(context, availableWidth);
        size = {control.x, size.y + control.y};
    }
    if (const std::string note = getNoteText(context); !note.empty()) {
        size.y += spacing + Typography::measureParagraph(context, Theme::Font::Caption, note, availableWidth).y;
    }
    return size;
}

void FormField::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing) * 0.5F;
    float y = bounds.y;
    if (!label.isEmpty()) {
        const std::string labelText = context.getText(label) + (required ? " *" : "");
        Typography::drawAligned(context, Theme::Font::Caption, {bounds.x, y, bounds.width, Typography::getLineHeight(context, Theme::Font::Caption)}, context.getColor(Theme::Color::TextMuted), labelText, Alignment::Start);
        y += Typography::getLineHeight(context, Theme::Font::Caption) + spacing;
    }
    if (!getChildren().empty()) {
        Component& control = *getChildren().front();
        const math::Vec2 size = control.measure(context, bounds.width);
        control.draw(context, {bounds.x, y, control.getAlignment() == Alignment::Stretch ? bounds.width : size.x, size.y});
        y += size.y + spacing;
    }
    if (const std::string note = getNoteText(context); !note.empty()) {
        const math::Vec2 size = Typography::measureParagraph(context, Theme::Font::Caption, note, bounds.width);
        Typography::drawParagraph(context, Theme::Font::Caption, {bounds.x, y, bounds.width, size.y}, context.getColor(error.isEmpty() ? Theme::Color::TextMuted : Theme::Color::DangerText), note, Alignment::Start);
    }
}

std::string FormField::getNoteText(Context& context) const {
    return context.getText(error.isEmpty() ? help : error);
}

} // namespace haylen::ui
