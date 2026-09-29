#include "ui/components/touch/TouchStick.hpp"

#include <optional>

#include "haylen/input/Input.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

void TouchStick::readProperties(PropertyReader& reader) {
    reader.read("action", action);
    reader.read("radius", radius, 8.0F, 2048.0F);
    reader.read("deadZone", deadZone, 0.0F, 0.95F);
    reader.read("floating", floating);
    reader.read("touchOnly", touchOnly);
    if (action.empty()) {
        reader.fail("action", "names the virtual stick to drive and cannot be empty");
    }
}

math::Vec2 TouchStick::measureContent(Context&, float) {
    return {radius * 2.0F, radius * 2.0F};
}

void TouchStick::render(Context& context, const math::Rect& bounds) {
    if (touchOnly && context.getInput().getLastDevice() != input::InputDevice::Touch) {
        tracker.reset();
        return;
    }
    context.blockPointer(bounds);

    const std::optional<math::Vec2> pointer = tracker.update(context, bounds);
    const math::Vec2 center = pointer && floating ? tracker.getStart() : bounds.getCenter();
    math::Vec2 value;
    if (pointer) {
        const math::Vec2 offset = ((*pointer - center) / radius).clampedLength(1.0F);
        const float length = offset.getLength();
        value = length <= deadZone ? math::Vec2{} : offset * ((length - deadZone) / (1.0F - deadZone) / length);
    }
    context.setStick(action, value);

    const math::Color base = context.getColor(Theme::Color::Overlay);
    const math::Rect ring{center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F};
    Surfaces::draw(context, Theme::Surface::StickBase, ring, base.withAlpha(pointer ? 0.45F : 0.3F), context.getColor(Theme::Color::BorderStrong), radius);
    const float knob = radius * 0.45F;
    const math::Vec2 knobCenter = center + value.clampedLength(1.0F) * (radius - knob);
    Surfaces::draw(context, Theme::Surface::StickKnob, {knobCenter.x - knob, knobCenter.y - knob, knob * 2.0F, knob * 2.0F}, context.getColor(Theme::Color::OnAccent).withAlpha(pointer ? 0.9F : 0.7F), std::nullopt, knob);
}

void TouchStick::drawingStopped(Context&) {
    tracker.reset();
}

} // namespace haylen::ui
