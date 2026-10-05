#include "ui/components/text/EmptyState.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void EmptyState::readProperties(PropertyReader& reader) {
    reader.read("image", image);
    reader.read("title", title);
    reader.read("message", message);
    reader.read("imageSize", imageSize, 0.0F, 4096.0F);
}

math::Vec2 EmptyState::measureContent(Context& context, float availableWidth) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    math::Vec2 size;
    if (!image.empty()) {
        size = {imageSize, imageSize + spacing};
    }
    const math::Vec2 titleSize = Typography::measureParagraph(context, Theme::Font::Heading, context.getText(title), availableWidth);
    const math::Vec2 messageSize = Typography::measureParagraph(context, Theme::Font::Body, context.getText(message), availableWidth);
    return {std::max({size.x, titleSize.x, messageSize.x}), size.y + titleSize.y + spacing * 0.5F + messageSize.y};
}

void EmptyState::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    float y = bounds.y;
    if (!image.empty()) {
        Surfaces::drawImage(context, context.getImage(image, {imageSize, imageSize}), {std::floor(bounds.getCenter().x - imageSize * 0.5F), y, imageSize, imageSize});
        y += imageSize + spacing;
    }
    const std::string titleText = context.getText(title);
    const float titleHeight = Typography::measureParagraph(context, Theme::Font::Heading, titleText, bounds.width).y;
    Typography::drawParagraph(context, Theme::Font::Heading, {bounds.x, y, bounds.width, titleHeight}, context.getColor(Theme::Color::Text), titleText, Alignment::Center);
    y += titleHeight + spacing * 0.5F;
    Typography::drawParagraph(context, Theme::Font::Body, {bounds.x, y, bounds.width, bounds.getBottom() - y}, context.getColor(Theme::Color::TextMuted), context.getText(message), Alignment::Center);
}

} // namespace haylen::ui
