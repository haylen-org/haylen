#include "ui/components/indicators/CircularProgress.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void CircularProgress::readProperties(PropertyReader& reader) {
    reader.read("value", value, 0.0F, 1.0F);
    reader.read("size", size, 0.0F, 2048.0F);
    reader.read("thickness", thickness, 0.0F, 512.0F);
    reader.readChoice<Variant>("variant", variant, kVariants);
    reader.readChoice<Widgets::Tone>("tone", tone, Widgets::kTones);
    reader.read("text", text);
    reader.read("image", image);
}

math::Vec2 CircularProgress::measureContent(Context& context, float) {
    const float side = size > 0.0F ? size : context.getMetric(Theme::Metric::CircularProgressSize);
    return {side, side};
}

void CircularProgress::render(Context& context, const math::Rect& bounds) {
    const float radius = std::min(bounds.width, bounds.height) * 0.5F;
    const math::Vec2 center = bounds.getCenter();
    if (variant == Variant::Ring) {
        drawRing(context, center, radius);
    } else {
        drawCooldown(context, bounds, center, radius);
    }

    const std::string shown = context.getText(text);
    if (shown.empty()) {
        return;
    }
    const Theme::Font font = radius < 40.0F ? Theme::Font::Caption : Theme::Font::Body;
    const float line = Typography::getLineHeight(context, font);
    const math::Rect label{center.x - radius, center.y - line * 0.5F, radius * 2.0F, line};
    if (variant == Variant::Ring) {
        Typography::drawAligned(context, font, label, context.getColor(Theme::Color::Text), shown, Alignment::Center);
    } else {
        Typography::drawParagraph(context, font, label, context.getColor(Theme::Color::Text), shown, Alignment::Center, context.getColor(Theme::Color::Window), 2.0F);
    }
}

// The track and the fill are bands along the edge of the circle, the fill cut to the share of the value.
void CircularProgress::drawRing(Context& context, math::Vec2 center, float radius) const {
    const float width = thickness > 0.0F ? thickness : context.getMetric(Theme::Metric::CircularProgressThickness);
    const math::Rect circle = math::Rect::fromCenter(center, {radius * 2.0F, radius * 2.0F});
    const std::array<float, 4> round{radius, radius, radius, radius};
    Surfaces::drawShape(context, {.bounds = circle, .radii = round, .color = math::Color::transparent(), .borderWidth = width, .borderColor = context.getColor(Theme::Color::Border)});
    if (value <= 0.0F) {
        return;
    }

    // The fill starts at the top and runs clockwise, which is the direction screen angles grow in.
    constexpr float kTop = -std::numbers::pi_v<float> * 0.5F;
    const Widgets::Tone fillTone = tone == Widgets::Tone::Neutral ? Widgets::Tone::Accent : tone;
    Surfaces::drawShape(context, {.bounds = circle, .radii = round, .startAngle = kTop, .sweep = std::numbers::pi_v<float> * 2.0F * value, .color = math::Color::transparent(), .borderWidth = width, .borderColor = context.getColor(Widgets::getToneColors(fillTone).fill)});
}

void CircularProgress::drawCooldown(Context& context, const math::Rect& bounds, math::Vec2 center, float radius) const {
    const math::Rect square{center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F};
    if (!image.empty()) {
        Surfaces::drawImage(context, context.getImage(image, square.getSize()), square);
    } else {
        Surfaces::draw(context, Theme::Surface::Badge, bounds, context.getColor(Widgets::getToneColors(tone).background), std::nullopt, radius);
    }
    if (value <= 0.0F) {
        return;
    }

    // The shade covers the sector of the circle still waiting and uncovers the picture clockwise from the top as the value falls.
    constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0F;
    const float start = -std::numbers::pi_v<float> * 0.5F + kTwoPi * (1.0F - value);
    Surfaces::drawShape(context, {.bounds = square, .radii = {radius, radius, radius, radius}, .startAngle = start, .sweep = kTwoPi * value, .color = context.getColor(Theme::Color::Overlay)});
}

} // namespace haylen::ui
