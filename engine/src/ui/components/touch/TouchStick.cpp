#include "ui/components/touch/TouchStick.hpp"

#include <algorithm>
#include <optional>

#include "haylen/input/Input.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

void TouchStick::readProperties(PropertyReader& reader) {
    reader.read("action", action);
    reader.read("radius", radius, 8.0F, 2048.0F);
    reader.read("deadZone", deadZone, 0.0F, 0.95F);
    reader.readChoice<Mode>("mode", mode, kModes);
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
        drawingStopped(context);
        return;
    }
    context.blockPointer(bounds);

    // A disabled stick rests at the center and waits for a new press once it is enabled again.
    if (!getCommon().enabled) {
        tracker.reset();
    }
    const std::optional<math::Vec2> pointer = getCommon().enabled ? tracker.update(context, bounds) : std::nullopt;
    if (!pointer) {
        held.reset();
    }
    const math::Vec2 center = pointer ? placeCenter(bounds, *pointer) : bounds.getCenter();
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

// The ring of a floating or following stick starts where the finger lands, and a following ring moves after a finger that leaves it, staying inside the area of the stick.
math::Vec2 TouchStick::placeCenter(const math::Rect& bounds, math::Vec2 pointer) {
    if (mode == Mode::Fixed) {
        return bounds.getCenter();
    }
    math::Vec2 center = held.value_or(tracker.getStart());
    const math::Vec2 offset = pointer - center;
    const float length = offset.getLength();
    if (mode == Mode::Following && length > radius) {
        center = pointer - offset * (radius / length);
        center = {std::clamp(center.x, bounds.x, bounds.getRight()), std::clamp(center.y, bounds.y, bounds.getBottom())};
    }
    held = center;
    return center;
}

void TouchStick::drawingStopped(Context&) {
    tracker.reset();
    held.reset();
}

} // namespace haylen::ui
