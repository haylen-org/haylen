#include "ui/components/touch/TouchButton.hpp"

#include <algorithm>
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
    const bool pressed = getCommon().enabled && tracker.update(context, bounds).has_value();
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
    if (const std::string shown = context.getText(text); !shown.empty()) {
        Typography::drawAligned(context, Theme::Font::Button, content, context.getColor(Theme::Color::OnAccent), shown, Alignment::Center);
    }
}

void TouchButton::drawingStopped(Context& context) {
    tracker.reset();
    if (std::exchange(down, false)) {
        context.emit(*this, "release");
    }
}

} // namespace haylen::ui
