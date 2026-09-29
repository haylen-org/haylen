#include "ui/components/indicators/Badge.hpp"

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Badge::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("solid", solid);
}

math::Vec2 Badge::measureContent(Context& context, float) {
    const math::Vec2 textSize = Typography::measure(context, Theme::Font::Caption, context.getText(text));
    const math::Insets image = Surfaces::getPadding(context, Theme::Surface::Badge);
    return {textSize.x + context.getMetric(Theme::Metric::BadgePaddingX) * 2.0F + image.getHorizontal(), textSize.y + context.getMetric(Theme::Metric::BadgePaddingY) * 2.0F + image.getVertical()};
}

void Badge::render(Context& context, const math::Rect& bounds) {
    const Widgets::ToneColors colors = Widgets::getToneColors(tone);
    Surfaces::draw(context, Theme::Surface::Badge, bounds, context.getColor(solid ? colors.fill : colors.background), std::nullopt, bounds.height * 0.5F);
    Typography::drawAligned(context, Theme::Font::Caption, bounds.inset(Surfaces::getPadding(context, Theme::Surface::Badge)), context.getColor(solid ? colors.ink : colors.text), context.getText(text), Alignment::Center);
}

} // namespace haylen::ui
