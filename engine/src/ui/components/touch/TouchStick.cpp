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

    const float knob = radius * 0.45F;
    const math::Vec2 knobCenter = center + value.clampedLength(1.0F) * (radius - knob);
    drawBase(context, center, pointer.has_value());
    drawKnob(context, knobCenter, knob, pointer.has_value());
}

// A theme without a picture of the base draws it as a ring with a guide ring where the knob turns, so the stick reads as a control and not a disc.
void TouchStick::drawBase(Context& context, math::Vec2 center, bool pressed) const {
    const math::Rect ring{center.x - radius, center.y - radius, radius * 2.0F, radius * 2.0F};
    const math::Color base = context.getColor(Theme::Color::Overlay);
    if (context.getSurface(Theme::Surface::StickBase) != nullptr) {
        Surfaces::draw(context, Theme::Surface::StickBase, ring, base.withAlpha(pressed ? 0.45F : 0.3F), context.getColor(Theme::Color::BorderStrong), radius);
        return;
    }
    const float border = context.getMetric(Theme::Metric::BorderWidth);
    Surfaces::drawShape(context, {.bounds = ring, .radii = {radius, radius, radius, radius}, .color = base.withAlpha(pressed ? 0.5F : 0.35F), .borderWidth = border * 1.5F, .borderColor = context.getColor(pressed ? Theme::Color::Accent : Theme::Color::BorderStrong)});
    const float guide = radius * 0.55F;
    Surfaces::outline(context, math::Rect::fromCenter(center, {guide * 2.0F, guide * 2.0F}), context.getColor(Theme::Color::BorderStrong).withAlpha(0.6F), guide, border);
}

// A theme without a picture of the knob draws it raised over a soft shadow, with a rim in the accent color.
void TouchStick::drawKnob(Context& context, math::Vec2 center, float knob, bool pressed) const {
    const math::Rect area{center.x - knob, center.y - knob, knob * 2.0F, knob * 2.0F};
    const math::Color fill = context.getColor(Theme::Color::OnAccent).withAlpha(pressed ? 0.95F : 0.8F);
    if (context.getSurface(Theme::Surface::StickKnob) != nullptr) {
        Surfaces::draw(context, Theme::Surface::StickKnob, area, fill, std::nullopt, knob);
        return;
    }
    Surfaces::drawShape(context, {.bounds = area.translated({0.0F, knob * 0.12F}), .radii = {knob, knob, knob, knob}, .color = context.getColor(Theme::Color::Shadow), .softness = knob * 0.3F});
    Surfaces::drawShape(context, {.bounds = area, .radii = {knob, knob, knob, knob}, .color = fill, .borderWidth = context.getMetric(Theme::Metric::BorderWidth) * 2.0F, .borderColor = context.getColor(Theme::Color::Accent)});
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
