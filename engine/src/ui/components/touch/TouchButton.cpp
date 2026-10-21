#include "ui/components/touch/TouchButton.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "haylen/input/Input.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

void TouchButton::readProperties(PropertyReader& reader) {
    reader.read("action", action);
    reader.read("text", text);
    reader.read("image", image);
    reader.read("size", size, 8.0F, 2048.0F);
    reader.read("touchOnly", touchOnly);
    reader.read("slideIn", slideIn);
    if (action.empty()) {
        reader.fail("action", "names the virtual button to drive and cannot be empty");
    }
}

math::Vec2 TouchButton::measureContent(Context&, float) {
    return {size, size};
}

void TouchButton::render(Context& context, const math::Rect& bounds) {
    if (touchOnly && context.getInput().getLastDevice() != input::InputDevice::Touch) {
        drawingStopped(context);
        return;
    }
    context.blockPointer(bounds);

    // A disabled button lets go of what it held and waits for a new press once it is enabled again.
    if (!getCommon().enabled) {
        tracker.reset();
    }
    const bool pressed = getCommon().enabled && tracker.update(context, bounds, slideIn).has_value();
    if (pressed) {
        context.holdButton(action);
    }
    if (pressed != down) {
        down = pressed;
        context.emit(*this, pressed ? "press" : "release");
    }

    const float radius = std::min(bounds.width, bounds.height) * 0.5F;
    const math::Rect circle{bounds.getCenter().x - radius, bounds.getCenter().y - radius, radius * 2.0F, radius * 2.0F};
    const math::Color fill = context.getColor(pressed ? Theme::Color::Accent : Theme::Color::Overlay).withAlpha(pressed ? 0.8F : 0.45F);
    Surfaces::draw(context, pressed ? Theme::Surface::TouchButtonPressed : Theme::Surface::TouchButton, circle, fill, context.getColor(Theme::Color::BorderStrong), radius);
    const math::Rect content = circle.translated({0.0F, pressed ? 2.0F : 0.0F});
    if (!image.empty()) {
        const float icon = radius;
        Surfaces::drawImage(context, context.getImage(image, {icon, icon}), {content.getCenter().x - icon * 0.5F, content.getCenter().y - icon * 0.5F, icon, icon});
    }
    if (const std::string_view shown = context.getText(text); !shown.empty()) {
        drawLabel(context, content, shown);
    }
}

// The label keeps a margin inside the ring along the chord at the top and bottom of its line, and a label wider than that draws smaller, down to half its size, and ends with an ellipsis when it still does not fit.
void TouchButton::drawLabel(Context& context, const math::Rect& circle, std::string_view shown) {
    const float radius = circle.width * 0.5F;
    const float lineHeight = Typography::getLineHeight(context, Theme::Font::Button);
    const float half = std::min(lineHeight * 0.5F, radius);
    const float room = std::max(0.0F, 2.0F * std::sqrt(radius * radius - half * half) - context.getMetric(Theme::Metric::ContentSpacing) * 2.0F);
    const float natural = Typography::measure(context, Theme::Font::Button, shown).x;
    const float scale = natural > room ? std::max(kMinLabelScale, room / natural) : 1.0F;
    const std::string_view fitted = natural * scale > room ? Typography::elide(context, Theme::Font::Button, shown, room / scale) : shown;
    const float height = lineHeight * scale;
    const math::Rect line{circle.x, std::floor(circle.getCenter().y - height * 0.5F), circle.width, height};
    Typography::drawParagraph(context, Theme::Font::Button, line, context.getColor(Theme::Color::OnAccent), fitted, Alignment::Center, std::nullopt, 2.0F, scale);
}

void TouchButton::drawingStopped(Context& context) {
    tracker.reset();
    if (std::exchange(down, false)) {
        context.emit(*this, "release");
    }
}

bool TouchButton::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::Button;
    element.label.assign(context.getText(text));
    element.actions = platform::AccessibilityElement::Activate;
    return true;
}

} // namespace haylen::ui
