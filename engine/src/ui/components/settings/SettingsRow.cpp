#include "ui/components/settings/SettingsRow.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void SettingsRow::readProperties(PropertyReader& reader) {
    reader.read("label", label);
    reader.read("caption", caption);
}

math::Vec2 SettingsRow::measureContent(Context& context, float availableWidth) {
    const float labelWidth = std::min(context.getMetric(Theme::Metric::SettingsLabelWidth), availableWidth * 0.5F);
    math::Vec2 text = Typography::measureParagraph(context, Theme::Font::Body, context.getText(label), labelWidth);
    if (const std::string captionText = context.getText(caption); !captionText.empty()) {
        text.y += Typography::measureParagraph(context, Theme::Font::Caption, captionText, labelWidth).y;
    }
    math::Vec2 control;
    if (!getChildren().empty()) {
        control = getChildren().front()->measure(context, std::max(0.0F, availableWidth - labelWidth));
    }
    const float padding = context.getMetric(Theme::Metric::ControlPaddingY);
    return {labelWidth + control.x, std::max({text.y, control.y, context.getMetric(Theme::Metric::ListRowHeight)}) + padding * 2.0F};
}

void SettingsRow::render(Context& context, const math::Rect& bounds) {
    const float padding = context.getMetric(Theme::Metric::ControlPaddingY);
    const math::Rect inner = bounds.inset({0.0F, padding, 0.0F, padding});
    const float labelWidth = std::min(context.getMetric(Theme::Metric::SettingsLabelWidth), bounds.width * 0.5F);

    const std::string labelText = context.getText(label);
    const std::string captionText = context.getText(caption);
    const float labelHeight = Typography::measureParagraph(context, Theme::Font::Body, labelText, labelWidth).y;
    const float captionHeight = captionText.empty() ? 0.0F : Typography::measureParagraph(context, Theme::Font::Caption, captionText, labelWidth).y;
    const float top = std::floor(inner.getCenter().y - (labelHeight + captionHeight) * 0.5F);
    Typography::drawParagraph(context, Theme::Font::Body, {inner.x, top, labelWidth, labelHeight}, context.getColor(Theme::Color::Text), labelText, Alignment::Start);
    if (!captionText.empty()) {
        Typography::drawParagraph(context, Theme::Font::Caption, {inner.x, top + labelHeight, labelWidth, captionHeight}, context.getColor(Theme::Color::TextMuted), captionText, Alignment::Start);
    }

    if (!getChildren().empty() && getChildren().front()->getCommon().visible) {
        Component& control = *getChildren().front();
        const float available = std::max(0.0F, inner.width - labelWidth - context.getMetric(Theme::Metric::ItemSpacing));
        const math::Vec2 size = control.measure(context, available);
        const float width = control.getAlignment() == Alignment::Stretch ? available : std::min(size.x, available);
        control.draw(context, {inner.getRight() - width, std::floor(inner.getCenter().y - size.y * 0.5F), width, size.y});
    }
}

} // namespace haylen::ui
