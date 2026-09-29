#include "ui/components/inputs/TextArea.hpp"

#include <algorithm>

#include "haylen/ui/Context.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void TextArea::readMore(PropertyReader& reader) {
    reader.read("rows", rows, 1, 200);
}

math::Vec2 TextArea::measureContent(Context& context, float availableWidth) {
    const float padding = context.getMetric(Theme::Metric::ControlPaddingY) * 2.0F;
    return {std::min(availableWidth, 480.0F), Typography::getLineHeight(context, Theme::Font::Body) * static_cast<float>(rows) + padding};
}

void TextArea::render(Context& context, const math::Rect& bounds) {
    drawEntry(context, bounds, platform::TextInput::Keyboard::Multiline);
}

} // namespace haylen::ui
