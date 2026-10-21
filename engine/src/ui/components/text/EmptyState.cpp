#include "ui/components/text/EmptyState.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "haylen/ui/AccessibilityTree.hpp"
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

// The picture, the title and the message stack in that order, and a part that is empty takes no room and no gap. The lines of text wrap at the reading width of the theme, so a long message never runs across a whole screen.
math::Vec2 EmptyState::measureContent(Context& context, float availableWidth) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float lines = std::min(availableWidth, context.getMetric(Theme::Metric::ReadingWidth));
    const std::string_view titleText = context.getText(title);
    const std::string_view messageText = context.getText(message);
    math::Vec2 size = image.empty() ? math::Vec2{} : math::Vec2{imageSize, imageSize};
    float gap = image.empty() ? 0.0F : spacing;
    if (!titleText.empty()) {
        const math::Vec2 measured = Typography::measureParagraph(context, Theme::Font::Heading, titleText, lines);
        size = {std::max(size.x, measured.x), size.y + gap + measured.y};
        gap = spacing * 0.5F;
    }
    if (!messageText.empty()) {
        const math::Vec2 measured = Typography::measureParagraph(context, Theme::Font::Body, messageText, lines);
        size = {std::max(size.x, measured.x), size.y + gap + measured.y};
    }
    return size;
}

void EmptyState::render(Context& context, const math::Rect& bounds) {
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float width = std::min(bounds.width, context.getMetric(Theme::Metric::ReadingWidth));
    const math::Rect column{std::floor(bounds.getCenter().x - width * 0.5F), bounds.y, width, bounds.height};
    float y = bounds.y;
    float gap = 0.0F;
    if (!image.empty()) {
        Surfaces::drawImage(context, context.getImage(image, {imageSize, imageSize}), {std::floor(bounds.getCenter().x - imageSize * 0.5F), y, imageSize, imageSize});
        y += imageSize;
        gap = spacing;
    }
    if (const std::string_view titleText = context.getText(title); !titleText.empty()) {
        y += gap;
        const float titleHeight = Typography::measureParagraph(context, Theme::Font::Heading, titleText, column.width).y;
        Typography::drawParagraph(context, Theme::Font::Heading, {column.x, y, column.width, titleHeight}, context.getColor(Theme::Color::Text), titleText, Alignment::Center);
        y += titleHeight;
        gap = spacing * 0.5F;
    }
    if (const std::string_view messageText = context.getText(message); !messageText.empty()) {
        y += gap;
        Typography::drawParagraph(context, Theme::Font::Body, {column.x, y, column.width, bounds.getBottom() - y}, context.getColor(Theme::Color::TextMuted), messageText, Alignment::Center);
    }
}

bool EmptyState::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::Text;
    AccessibilityTree::appendParts(element.label, {context.getText(title), context.getText(message)});
    return true;
}

} // namespace haylen::ui
