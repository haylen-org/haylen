#include "ui/components/text/PageHeader.hpp"

#include <algorithm>
#include <string>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void PageHeader::readProperties(PropertyReader& reader) {
    reader.read("title", title);
    reader.read("caption", caption);
    reader.read("banner", banner);
    reader.readChoice<Alignment>("textAlign", textAlign, Typography::kAlignments);
}

math::Vec2 PageHeader::measureContent(Context& context, float availableWidth) {
    const math::Insets padding = getInsets(context);
    const float width = std::max(0.0F, availableWidth - padding.getHorizontal());
    math::Vec2 size = Typography::measureParagraph(context, Theme::Font::Title, context.getText(title), width);
    if (const std::string captionText = context.getText(caption); !captionText.empty()) {
        const math::Vec2 captionSize = Typography::measureParagraph(context, Theme::Font::Body, captionText, width);
        size = {std::max(size.x, captionSize.x), size.y + captionSize.y + context.getMetric(Theme::Metric::ItemSpacing) * 0.25F};
    }
    return {size.x + padding.getHorizontal(), size.y + padding.getVertical()};
}

void PageHeader::render(Context& context, const math::Rect& bounds) {
    if (banner) {
        Surfaces::draw(context, Theme::Surface::Banner, bounds, context.getColor(Theme::Color::Accent), std::nullopt);
    }
    const math::Rect inner = bounds.inset(getInsets(context));
    const std::string titleText = context.getText(title);
    const float titleHeight = Typography::measureParagraph(context, Theme::Font::Title, titleText, inner.width).y;
    Typography::drawParagraph(context, Theme::Font::Title, {inner.x, inner.y, inner.width, titleHeight}, context.getColor(banner ? Theme::Color::OnAccent : Theme::Color::Text), titleText, textAlign);
    if (const std::string captionText = context.getText(caption); !captionText.empty()) {
        const float top = inner.y + titleHeight + context.getMetric(Theme::Metric::ItemSpacing) * 0.25F;
        Typography::drawParagraph(context, Theme::Font::Body, {inner.x, top, inner.width, inner.getBottom() - top}, context.getColor(banner ? Theme::Color::OnAccent : Theme::Color::TextMuted), captionText, textAlign);
    }
}

math::Insets PageHeader::getInsets(Context& context) const {
    if (!banner) {
        return {};
    }
    const math::Insets image = Surfaces::getPadding(context, Theme::Surface::Banner);
    const float x = context.getMetric(Theme::Metric::PanelPadding);
    const float y = context.getMetric(Theme::Metric::ControlPaddingY);
    return {image.left + x, image.top + y, image.right + x, image.bottom + y};
}

} // namespace haylen::ui
