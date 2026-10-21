#include "ui/Widgets.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <optional>

#include <imgui_internal.h>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

math::Color Widgets::mix(math::Color base, math::Color overlay) noexcept {
    const float amount = overlay.a;
    return {base.r + (overlay.r - base.r) * amount, base.g + (overlay.g - base.g) * amount, base.b + (overlay.b - base.b) * amount, std::max(base.a, amount)};
}

Widgets::SurfaceSet Widgets::getSurfaces(ButtonVariant variant) noexcept {
    switch (variant) {
    case ButtonVariant::Primary:
        return {Theme::Surface::ButtonPrimary, Theme::Surface::ButtonPrimaryHover, Theme::Surface::ButtonPrimaryPressed};
    case ButtonVariant::Destructive:
        return {Theme::Surface::ButtonDestructive, Theme::Surface::ButtonDestructiveHover, Theme::Surface::ButtonDestructivePressed};
    default:
        return {Theme::Surface::Button, Theme::Surface::ButtonHover, Theme::Surface::ButtonPressed};
    }
}

const Theme::Image* Widgets::getStateImage(Context& context, const SurfaceSet& set, const Interaction& state) {
    if (state.held && context.getSurface(set.pressed) != nullptr) {
        return context.getSurface(set.pressed);
    }
    if (state.hovered && context.getSurface(set.hover) != nullptr) {
        return context.getSurface(set.hover);
    }
    return context.getSurface(set.normal);
}

Widgets::ToneColors Widgets::getToneColors(Tone tone) noexcept {
    switch (tone) {
    case Tone::Accent:
        return {Theme::Color::Accent, Theme::Color::OnAccent, Theme::Color::AccentBackground, Theme::Color::AccentText};
    case Tone::Success:
        return {Theme::Color::Success, Theme::Color::OnSuccess, Theme::Color::SuccessBackground, Theme::Color::SuccessText};
    case Tone::Warning:
        return {Theme::Color::Warning, Theme::Color::OnWarning, Theme::Color::WarningBackground, Theme::Color::WarningText};
    case Tone::Danger:
        return {Theme::Color::Danger, Theme::Color::OnDanger, Theme::Color::DangerBackground, Theme::Color::DangerText};
    case Tone::Information:
        return {Theme::Color::Information, Theme::Color::OnInformation, Theme::Color::InformationBackground, Theme::Color::InformationText};
    case Tone::Neutral:
        break;
    }
    return {Theme::Color::BorderStrong, Theme::Color::Text, Theme::Color::Hover, Theme::Color::TextMuted};
}

Widgets::Interaction Widgets::interact(Context& context, const math::Rect& bounds, float radius, std::string_view label, ImGuiButtonFlags flags) {
    const ImGuiID id = ImGui::GetID(label.data(), label.data() + label.size());
    const ImRect box = ImGuiConverter::toImRect(bounds);

    // Items scrolled out of view still register, so navigation can reach them and scroll them in. Items that never take the focus, such as the arrows of a stepper, stay out of navigation.
    const bool shown = ImGui::ItemAdd(box, id);
    if ((flags & ImGuiButtonFlags_NoNavFocus) == 0) {
        context.getFocus().addTarget(id, bounds);
    }
    if (!shown) {
        return {};
    }

    // A control that cannot take the focus keeps it where it was when clicked, so a key that also plays the game never presses it.
    ImGuiButtonFlags behavior = flags;
    if (!context.getFocus().isFocusable()) {
        behavior |= ImGuiButtonFlags_NoNavFocus;
    }
    Interaction state;
    state.clicked = ImGui::ButtonBehavior(box, id, &state.hovered, &state.held, behavior);
    drawFocusRing(context, bounds, id, radius);
    return state;
}

bool Widgets::isVisible(const Context& context, const math::Rect& bounds) {
    const math::Rect shown = context.getReshape().apply(bounds);
    return ImGui::IsRectVisible(ImGuiConverter::toImVec2(shown.getMin()), ImGuiConverter::toImVec2(shown.getMax()));
}

void Widgets::drawFocusRing(Context& context, const math::Rect& bounds, ImGuiID id, float radius) {
    if (!context.getFocus().isRingShown(id)) {
        return;
    }
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Context::Reshape shape = context.getReshape();
    if (shape.scale.x == 0.0F || shape.scale.y == 0.0F) {
        return;
    }

    // The line runs a gap away from the edge of the control, and it may reach past the clip of a scrolled area so an item at the edge still shows it whole. The ring lies in the units of the control, which the transforms around it move onto the screen, where the clip of the window lies.
    const float width = context.getMetric(Theme::Metric::FocusWidth);
    const float distance = context.getMetric(Theme::Metric::FocusGap) + width;
    const math::Rect clip = Context::Reshape{.scale = math::Vec2{1.0F, 1.0F} / shape.scale, .offset = math::Vec2{} - shape.offset / shape.scale}.apply(ImGuiConverter::toRect(window->ClipRect));
    const math::Rect ring = bounds.intersection(clip).expanded(distance);
    const bool clipped = !clip.contains(ring);
    if (clipped) {
        const math::Rect shown = shape.apply(ring);
        window->DrawList->PushClipRect(ImGuiConverter::toImVec2(shown.getMin()), ImGuiConverter::toImVec2(shown.getMax()));
    }
    // A theme with a focus ring surface frames the control with its image instead of the line.
    const float rounding = radius + distance;
    if (const Theme::Image* image = context.getSurface(Theme::Surface::FocusRing)) {
        Surfaces::drawNineSlice(context, *image, ring, context.getColor(Theme::Color::Focus));
    } else {
        Surfaces::drawShape(context, {.bounds = ring, .radii = {rounding, rounding, rounding, rounding}, .color = math::Color::transparent(), .borderWidth = width, .borderColor = context.getColor(Theme::Color::Focus)});
    }
    if (clipped) {
        window->DrawList->PopClipRect();
    }
}

void Widgets::focusItem(Context& context) {
    const ImRect& box = GImGui->LastItemData.Rect;
    context.getFocus().focus(ImGui::GetItemID(), math::Rect::fromMinMax({box.Min.x, box.Min.y}, {box.Max.x, box.Max.y}));
}

math::Vec2 Widgets::measureButton(Context& context, std::string_view label, bool hasIcon, ButtonVariant variant, const ButtonContent& content, float availableWidth) {
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    const float icon = context.getMetric(Theme::Metric::IconSize);
    if (variant == ButtonVariant::Icon) {
        return {height, height};
    }
    const float textWidth = label.empty() ? 0.0F : Typography::measure(context, Theme::Font::Button, label).x;
    if (variant == ButtonVariant::Link) {
        return {textWidth, Typography::getLineHeight(context, Theme::Font::Button)};
    }

    const math::Insets padding = Surfaces::getPadding(context, getSurfaces(variant).normal);
    const float paddingX = context.getMetric(Theme::Metric::ControlPaddingX) * 2.0F + padding.getHorizontal();
    if (content.iconPlacement == IconPlacement::Top && hasIcon) {
        const float labelHeight = label.empty() ? 0.0F : Typography::getLineHeight(context, Theme::Font::Button) + context.getMetric(Theme::Metric::ContentSpacing) * 0.5F;
        const float stacked = icon + labelHeight + context.getMetric(Theme::Metric::ControlPaddingY) + padding.getVertical();
        return {std::max(std::max(textWidth, icon) + paddingX, height), std::max(stacked, height)};
    }
    const float iconRoom = hasIcon ? icon + (label.empty() ? 0.0F : context.getMetric(Theme::Metric::ContentSpacing)) : 0.0F;
    const float room = std::max(0.0F, availableWidth - paddingX - iconRoom);
    if (textWidth > room && room > 0.0F) {
        const math::Vec2 wrapped = Typography::measure(context, Theme::Font::Button, label, room);
        const float wrappedHeight = wrapped.y + context.getMetric(Theme::Metric::ControlPaddingY) * 2.0F + padding.getVertical();
        return {std::max(wrapped.x + paddingX + iconRoom, height), std::max(wrappedHeight, height)};
    }
    return {std::max(textWidth + paddingX + iconRoom, height), height};
}

bool Widgets::button(Context& context, const math::Rect& bounds, std::string_view label, const graphics::Texture* icon, ButtonVariant variant, bool checked, const IconColor& iconColor, const ButtonContent& content) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius));

    if (variant == ButtonVariant::Link) {
        const math::Color color = context.getColor(state.hovered ? Theme::Color::AccentHover : Theme::Color::AccentText);
        Typography::drawAligned(context, Theme::Font::Button, bounds, color, label, Alignment::Start);
        if (state.hovered) {
            const float y = std::floor(bounds.y + (bounds.height + Typography::getLineHeight(context, Theme::Font::Button)) * 0.5F);
            const math::Rect line = context.mirror({bounds.x, y, std::min(Typography::measure(context, Theme::Font::Button, label).x, bounds.width), 0.0F}, bounds);
            Widgets::line(context, {line.x, y}, {line.getRight(), y}, 1.0F, color);
        }
        return state.clicked;
    }

    const SurfaceSet surfaces = getSurfaces(variant);
    const Theme::Image* textured = getStateImage(context, surfaces, state);
    math::Color fill = context.getColor(Theme::Color::Raised);
    math::Color textColor = context.getColor(Theme::Color::Text);
    std::optional<math::Color> border = context.getColor(Theme::Color::Border);
    if (variant == ButtonVariant::Primary || variant == ButtonVariant::Destructive) {
        const bool primary = variant == ButtonVariant::Primary;
        fill = context.getColor(state.held ? (primary ? Theme::Color::AccentStrong : Theme::Color::DangerStrong) : state.hovered ? (primary ? Theme::Color::AccentHover : Theme::Color::DangerHover) : (primary ? Theme::Color::Accent : Theme::Color::Danger));
        textColor = context.getColor(primary ? Theme::Color::OnAccent : Theme::Color::OnDanger);
        border.reset();
    } else if (variant == ButtonVariant::Toolbar || variant == ButtonVariant::Icon) {
        fill = checked ? context.getColor(Theme::Color::Selection) : math::Color::transparent();
        border.reset();
    }
    if (variant != ButtonVariant::Primary && variant != ButtonVariant::Destructive) {
        if (state.held) {
            fill = mix(fill, context.getColor(Theme::Color::Pressed));
        } else if (state.hovered) {
            fill = mix(fill, context.getColor(Theme::Color::Hover));
        }
    }

    if (textured != nullptr) {
        Surfaces::drawNineSlice(context, *textured, bounds, fill);
    } else {
        Surfaces::draw(context, surfaces.normal, bounds, fill, border);
    }
    if (checked && textured != nullptr) {
        Surfaces::fill(context, bounds, context.getColor(Theme::Color::Selection), context.getMetric(Theme::Metric::ControlRadius));
    }

    const math::Color iconTint = iconColor.resolve(context, variant == ButtonVariant::Icon || variant == ButtonVariant::Toolbar ? textColor : math::Color::white());
    drawButtonContent(context, bounds.inset(Surfaces::getPadding(context, surfaces.normal)), label, icon != nullptr && icon->isValid() ? icon : nullptr, textColor, iconTint, content);
    return state.clicked;
}

// The icon and the label sit together where `justify` puts them, the icon on the side the UI starts or above the label, and away from the edges by the control padding unless they sit in the middle.
void Widgets::drawButtonContent(Context& context, const math::Rect& inner, std::string_view label, const graphics::Texture* icon, math::Color textColor, math::Color iconColor, const ButtonContent& content) {
    const float iconSize = icon != nullptr ? context.getMetric(Theme::Metric::IconSize) : 0.0F;
    const float spacing = context.getMetric(Theme::Metric::ContentSpacing);
    const float edge = content.justify == Alignment::Center ? 0.0F : std::min(context.getMetric(Theme::Metric::ControlPaddingX), inner.width * 0.5F);
    const float start = inner.x + edge;
    const float room = std::max(0.0F, inner.width - edge * 2.0F);
    const float measured = label.empty() ? 0.0F : Typography::measure(context, Theme::Font::Button, label).x;

    if (content.iconPlacement == IconPlacement::Top && iconSize > 0.0F) {
        const float lineHeight = label.empty() ? 0.0F : Typography::getLineHeight(context, Theme::Font::Button);
        const float gap = label.empty() ? 0.0F : spacing * 0.5F;
        const float top = std::floor(inner.getCenter().y - (iconSize + gap + lineHeight) * 0.5F);
        const float iconX = std::floor(Component::align(content.justify, start, room, iconSize));
        Surfaces::drawImage(context, *icon, context.mirror({iconX, top, iconSize, iconSize}, inner), iconColor);
        if (!label.empty()) {
            const float labelWidth = std::min(measured, room);
            const float labelX = std::floor(Component::align(content.justify, start, room, labelWidth));
            Typography::drawAligned(context, Theme::Font::Button, context.mirror({labelX, top + iconSize + gap, labelWidth, lineHeight}, inner), textColor, label, Alignment::Start);
        }
        return;
    }

    // A label wider than its room wraps there, on lines that keep the alignment of the content.
    const float textRoom = std::max(0.0F, room - (iconSize > 0.0F ? iconSize + spacing : 0.0F));
    const bool wrapped = measured > textRoom && textRoom > 0.0F;
    const math::Vec2 text = wrapped ? Typography::measure(context, Theme::Font::Button, label, textRoom) : math::Vec2{std::min(measured, textRoom), Typography::getLineHeight(context, Theme::Font::Button)};
    const float contentWidth = iconSize + text.x + (iconSize > 0.0F && text.x > 0.0F ? spacing : 0.0F);
    float x = std::floor(Component::align(content.justify, start, room, contentWidth));
    if (iconSize > 0.0F) {
        Surfaces::drawImage(context, *icon, context.mirror({x, std::floor(inner.getCenter().y - iconSize * 0.5F), iconSize, iconSize}, inner), iconColor);
        x += iconSize + spacing;
    }
    if (wrapped) {
        Typography::drawParagraph(context, Theme::Font::Button, context.mirror({x, std::floor(inner.getCenter().y - text.y * 0.5F), text.x, text.y}, inner), textColor, label, content.justify == Alignment::Center ? Alignment::Center : Alignment::Start);
    } else if (text.x > 0.0F) {
        Typography::drawAligned(context, Theme::Font::Button, context.mirror({x, inner.y, text.x, inner.height}, inner), textColor, label, Alignment::Start);
    }
}

math::Vec2 Widgets::measureChoice(Context& context, std::string_view label, float availableWidth) {
    const float box = context.getMetric(Theme::Metric::ChoiceSize);
    return measureChoiceRow(context, label, box, box, availableWidth);
}

math::Vec2 Widgets::measureChoiceRow(Context& context, std::string_view label, float mark, float markHeight, float availableWidth) {
    const float spacing = context.getMetric(Theme::Metric::ContentSpacing);
    const math::Vec2 text = label.empty() ? math::Vec2{0.0F, Typography::getLineHeight(context, Theme::Font::Body)} : Typography::measure(context, Theme::Font::Body, label, std::max(1.0F, availableWidth - mark - spacing));
    const float width = mark + (label.empty() ? 0.0F : spacing + text.x);
    return {width, std::max(std::max(markHeight, text.y) + context.getMetric(Theme::Metric::ControlPaddingY), context.getMetric(Theme::Metric::ControlHeight))};
}

void Widgets::drawChoiceLabel(Context& context, const math::Rect& bounds, float mark, std::string_view label) {
    if (label.empty()) {
        return;
    }
    const float spacing = context.getMetric(Theme::Metric::ContentSpacing);
    const float width = std::max(0.0F, bounds.width - mark - spacing);
    const float height = Typography::measure(context, Theme::Font::Body, label, width).y;
    const math::Rect area{bounds.x + mark + spacing, std::floor(bounds.getCenter().y - height * 0.5F), width, height};
    Typography::drawParagraph(context, Theme::Font::Body, context.mirror(area, bounds), context.getColor(Theme::Color::Text), label, Alignment::Start);
}

bool Widgets::checkbox(Context& context, const math::Rect& bounds, bool& value, std::string_view label) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius));
    if (state.clicked) {
        value = !value;
    }

    const float size = context.getMetric(Theme::Metric::ChoiceSize);
    const math::Rect box = context.mirror({bounds.x, std::floor(bounds.getCenter().y - size * 0.5F), size, size}, bounds);
    const Theme::Surface role = value ? Theme::Surface::CheckChecked : Theme::Surface::Check;
    math::Color fill = context.getColor(value ? Theme::Color::Accent : Theme::Color::Raised);
    if (state.hovered) {
        fill = mix(fill, context.getColor(Theme::Color::Hover));
    }
    Surfaces::draw(context, role, box, fill, context.getColor(value ? Theme::Color::Accent : Theme::Color::BorderStrong), context.getMetric(Theme::Metric::CheckRadius));
    if (value && context.getSurface(role) == nullptr) {
        const float padding = size * 0.22F;
        checkMark(context, {box.x + padding, box.y + padding}, size - padding * 2.0F, context.getColor(Theme::Color::OnAccent));
    }
    drawChoiceLabel(context, bounds, size, label);
    return state.clicked;
}

bool Widgets::radio(Context& context, const math::Rect& bounds, bool selected, std::string_view label, std::string_view idLabel) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius), idLabel);
    const float size = context.getMetric(Theme::Metric::ChoiceSize);
    const math::Rect circle = context.mirror({bounds.x, bounds.getCenter().y - size * 0.5F, size, size}, bounds);
    math::Color fill = context.getColor(Theme::Color::Raised);
    if (state.hovered) {
        fill = mix(fill, context.getColor(Theme::Color::Hover));
    }
    const Theme::Image* image = context.getSurface(selected ? Theme::Surface::RadioSelected : Theme::Surface::Radio);
    if (image != nullptr) {
        Surfaces::drawNineSlice(context, *image, circle, selected ? context.getColor(Theme::Color::Accent) : fill);
    } else {
        // The ring runs inside the circle of the mark, so it never reaches past its size.
        const float half = size * 0.5F;
        Surfaces::drawShape(context, {.bounds = circle, .radii = {half, half, half, half}, .color = fill, .borderWidth = context.getMetric(Theme::Metric::BorderWidth), .borderColor = context.getColor(selected ? Theme::Color::Accent : Theme::Color::BorderStrong)});
    }
    if (selected && image == nullptr) {
        const float dot = size * 0.25F;
        Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(circle.getCenter(), {dot * 2.0F, dot * 2.0F}), .radii = {dot, dot, dot, dot}, .color = context.getColor(Theme::Color::Accent)});
    }
    drawChoiceLabel(context, bounds, size, label);
    return state.clicked;
}

math::Vec2 Widgets::measureToggle(Context& context, std::string_view label, float availableWidth) {
    return measureChoiceRow(context, label, context.getMetric(Theme::Metric::ToggleWidth), context.getMetric(Theme::Metric::ToggleHeight), availableWidth);
}

bool Widgets::toggle(Context& context, const math::Rect& bounds, bool& value, std::string_view label) {
    const Interaction state = interact(context, bounds, bounds.height * 0.5F);
    if (state.clicked) {
        value = !value;
    }

    // The knob slides toward its side over the transition duration of the theme, and ImGui storage keeps where it was.
    float& position = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetItemID(), value ? 1.0F : 0.0F);
    const float target = value ? 1.0F : 0.0F;
    const float duration = context.getMetric(Theme::Metric::TransitionDuration);
    const float step = duration > 0.0F ? context.getDeltaSeconds() / duration : 1.0F;
    position = position < target ? std::min(target, position + step) : std::max(target, position - step);
    const float eased = position * position * (3.0F - 2.0F * position);

    // The track never moves or changes its shape: the on color fades in over the whole groove while the knob slides, and a right-to-left UI mirrors the switch, which then turns on toward the left.
    const float width = context.getMetric(Theme::Metric::ToggleWidth);
    const float height = context.getMetric(Theme::Metric::ToggleHeight);
    const math::Rect track = context.mirror({bounds.x, std::floor(bounds.getCenter().y - height * 0.5F), width, height}, bounds);
    const std::optional<math::Color> layer = state.held ? std::optional(context.getColor(Theme::Color::Pressed)) : state.hovered ? std::optional(context.getColor(Theme::Color::Hover)) : std::nullopt;
    const math::Color off = layer ? mix(context.getColor(Theme::Color::Track), *layer) : context.getColor(Theme::Color::Track);
    const math::Color on = layer ? mix(context.getColor(Theme::Color::Accent), *layer) : context.getColor(Theme::Color::Accent);
    if (context.getSurface(Theme::Surface::Track) == nullptr && context.getSurface(Theme::Surface::TrackFill) == nullptr) {
        // Flat colors fade the on color into the track itself, so the edge of the track never shows a rim of its own color around a fill on top of it.
        Surfaces::draw(context, Theme::Surface::Track, track, mix(off, on.withAlpha(on.a * eased)), std::nullopt, height * 0.5F);
    } else {
        Surfaces::draw(context, Theme::Surface::Track, track, off, std::nullopt, height * 0.5F);
        if (eased > 0.0F) {
            const math::Rect groove = track.inset(Surfaces::getPadding(context, Theme::Surface::Track));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * eased);
            Surfaces::draw(context, Theme::Surface::TrackFill, groove, on, std::nullopt, groove.height * 0.5F);
            ImGui::PopStyleVar();
        }
    }

    const float inset = std::min(context.getMetric(Theme::Metric::ToggleKnobInset), height * 0.5F);
    const float knob = height - inset * 2.0F;
    const math::Rect knobRect = context.mirror({track.x + inset + (width - knob - inset * 2.0F) * eased, track.y + inset, knob, knob}, track);
    const math::Color knobColor = context.getColor(Theme::Color::Knob);
    Surfaces::draw(context, Theme::Surface::Knob, knobRect, state.held ? mix(knobColor, context.getColor(Theme::Color::Pressed)) : knobColor, std::nullopt, knob * 0.5F);
    drawChoiceLabel(context, bounds, width, label);
    return state.clicked;
}

bool Widgets::slider(Context& context, const math::Rect& bounds, double& value, double minimum, double maximum, double step) {
    ImGuiContext& state = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID id = ImGui::GetID("##slider");
    const ImRect box = ImGuiConverter::toImRect(bounds);
    const bool shown = ImGui::ItemAdd(box, id);
    context.getFocus().addTarget(id, bounds);
    if (!shown) {
        return false;
    }

    // The pointer drags the knob the way ImGui's own slider does, while the focus moves it with left and right through the component.
    const bool hovered = ImGui::ItemHoverable(box, id, state.LastItemData.ItemFlags);
    const bool clicked = hovered && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
    if (clicked) {
        ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
        ImGui::SetActiveID(id, window);
        if (context.getFocus().isFocusable()) {
            ImGui::SetFocusID(id, window);
        }
        ImGui::FocusWindow(window);
    }

    // A right-to-left slider grows toward the left, which ImGui drags as a reversed range.
    const float knob = context.getMetric(Theme::Metric::SliderKnobSize);
    const bool reversed = context.isRightToLeft();
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, knob);
    ImRect grab;
    const double before = value;
    ImGui::SliderBehavior(box, id, ImGuiDataType_Double, &value, reversed ? &maximum : &minimum, reversed ? &minimum : &maximum, "%.4f", ImGuiSliderFlags_NoInput, &grab);
    ImGui::PopStyleVar();
    if (GImGui->ActiveId == id) {
        context.keepDrag(id);
    }

    // Only a value the player moves lands on the step, so a value given off the step stays as it is until then.
    if (value != before) {
        value = snap(value, minimum, maximum, step);
    }
    drawFocusRing(context, bounds, id, bounds.height * 0.5F);

    const float trackHeight = context.getMetric(Theme::Metric::SliderTrackHeight);
    const float span = maximum > minimum ? static_cast<float>((value - minimum) / (maximum - minimum)) : 0.0F;
    const math::Rect track{bounds.x + knob * 0.5F, std::floor(bounds.getCenter().y - trackHeight * 0.5F), bounds.width - knob, trackHeight};
    const math::Rect groove = track.inset(Surfaces::getPadding(context, Theme::Surface::Track));
    Surfaces::draw(context, Theme::Surface::Track, track, context.getColor(Theme::Color::BorderStrong), std::nullopt, trackHeight * 0.5F);
    Surfaces::draw(context, Theme::Surface::TrackFill, context.mirror({groove.x, groove.y, groove.width * span, groove.height}, groove), context.getColor(Theme::Color::Accent), std::nullopt, groove.height * 0.5F);
    const math::Rect knobRect = context.mirror({track.x + track.width * span - knob * 0.5F, std::floor(bounds.getCenter().y - knob * 0.5F), knob, knob}, track);
    const bool active = state.ActiveId == id;
    Surfaces::draw(context, Theme::Surface::Knob, knobRect, active || hovered ? mix(context.getColor(Theme::Color::OnAccent), context.getColor(Theme::Color::Pressed)) : context.getColor(Theme::Color::OnAccent), context.getColor(Theme::Color::Accent), knob * 0.5F);

    const bool changed = value != before;
    if (changed) {
        ImGui::MarkItemEdited(id);
    }
    return changed;
}

double Widgets::snap(double value, double minimum, double maximum, double step) noexcept {
    if (step <= 0.0) {
        return std::clamp(value, minimum, maximum);
    }
    return std::clamp(minimum + std::round((value - minimum) / step) * step, minimum, maximum);
}

void Widgets::progress(Context& context, const math::Rect& bounds, float value, Tone tone) {
    const float amount = std::clamp(value, 0.0F, 1.0F);
    Surfaces::draw(context, Theme::Surface::Track, bounds, context.getColor(Theme::Color::Border), std::nullopt, bounds.height * 0.5F);
    if (amount <= 0.0F) {
        return;
    }

    // The fill stays inside the padding of the track surface, so a framed bar image keeps its frame visible.
    const math::Rect groove = bounds.inset(Surfaces::getPadding(context, Theme::Surface::Track));
    const math::Rect filled = context.mirror({groove.x, groove.y, std::max(groove.width * amount, context.getSurface(Theme::Surface::TrackFill) != nullptr ? 0.0F : groove.height), groove.height}, groove);
    Surfaces::draw(context, Theme::Surface::TrackFill, filled, context.getColor(getToneColors(tone == Tone::Neutral ? Tone::Accent : tone).fill), std::nullopt, groove.height * 0.5F);
}

// The stroke is a band along the edge of a circle that reaches half its width past the radius, cut to three quarters of a turn.
void Widgets::spinner(Context& context, math::Vec2 center, float radius, math::Color color) {
    constexpr float kArc = std::numbers::pi_v<float> * 1.5F;
    const auto start = static_cast<float>(std::fmod(context.getTime() * 6.0, 2.0 * std::numbers::pi));
    const float stroke = std::max(2.0F, radius * 0.2F);
    const float outer = radius + stroke * 0.5F;
    Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(center, {outer * 2.0F, outer * 2.0F}), .radii = {outer, outer, outer, outer}, .startAngle = start, .sweep = kArc, .color = math::Color::transparent(), .borderWidth = stroke, .borderColor = color});
}

// The mark is the corner of a rectangle turned by an eighth of a turn, whose border along two sides forms the two strokes, with the quarter of the rectangle around that corner kept. The short stroke runs a third of the size up and to the left of the corner and the long one twice as far up and to the right, the way ImGui draws its check mark.
void Widgets::checkMark(Context& context, math::Vec2 origin, float size, math::Color color) {
    constexpr float kTurn = std::numbers::pi_v<float> * 0.25F;
    const float stroke = std::max(size / 5.0F, 1.0F);
    const float side = size - stroke * 0.5F;
    const float third = side / 3.0F;
    const math::Vec2 corner{origin.x + stroke * 0.25F + third, origin.y + stroke * 0.25F + side - third * 0.5F};
    const float shortArm = third * std::numbers::sqrt2_v<float>;
    const float longArm = shortArm * 2.0F;
    const math::Vec2 reach{(shortArm - longArm) * std::cos(kTurn), (shortArm + longArm) * std::sin(kTurn)};
    const math::Vec2 extent{(shortArm + stroke * 0.5F) * 2.0F, (longArm + stroke * 0.5F) * 2.0F};
    Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(corner - reach, extent), .rotation = kTurn, .sweep = std::numbers::pi_v<float> * 0.5F, .color = math::Color::transparent(), .borderWidth = stroke, .borderColor = color});
}

// A line covers its width to the right of and below its points, from one point to the other, so a horizontal or vertical line on whole units covers whole units and a line along the top or left edge of an area stays inside it.
void Widgets::line(Context& context, math::Vec2 from, math::Vec2 to, float width, math::Color color) {
    const math::Vec2 delta = to - from;
    const float length = delta.getLength();
    if (length <= 0.0F) {
        return;
    }
    const math::Vec2 side = math::Vec2{std::fabs(delta.y), std::fabs(delta.x)} * (width * 0.5F / length);
    const math::Vec2 middle = (from + to) * 0.5F + side;
    Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(middle, {length, width}), .rotation = delta.getAngle(), .color = color});
}

void Widgets::cross(Context& context, math::Vec2 center, float arm, float width, math::Color color) {
    line(context, {center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, width, color);
    line(context, {center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, width, color);
}

bool Widgets::closeImage(Context& context, const math::Rect& area, const Interaction& state) {
    const Theme::Image* image = getStateImage(context, {Theme::Surface::Close, Theme::Surface::CloseHover, Theme::Surface::ClosePressed}, state);
    if (image == nullptr) {
        return false;
    }
    Surfaces::drawNineSlice(context, *image, area, context.getColor(Theme::Color::Danger));
    return true;
}

ImGuiDir Widgets::mirror(const Context& context, ImGuiDir direction) noexcept {
    if (!context.isRightToLeft() || (direction != ImGuiDir_Left && direction != ImGuiDir_Right)) {
        return direction;
    }
    return direction == ImGuiDir_Left ? ImGuiDir_Right : ImGuiDir_Left;
}

int Widgets::getStep(const Context& context, FocusDirection direction) noexcept {
    return (direction == FocusDirection::Right) != context.isRightToLeft() ? 1 : -1;
}

// The triangle is the part of a rectangle around its tip that the sector reaching the two far corners keeps, so the far side of the rectangle is its base.
void Widgets::arrow(Context& context, math::Vec2 center, float size, ImGuiDir direction, math::Color color) {
    constexpr float kPi = std::numbers::pi_v<float>;
    const float pointing = direction == ImGuiDir_Down ? kPi * 0.5F : direction == ImGuiDir_Up ? -kPi * 0.5F : direction == ImGuiDir_Left ? kPi : 0.0F;
    const math::Vec2 tip = center + math::Vec2::fromAngle(pointing, size * 0.5F);
    const float spread = std::atan2(size * 0.5F, size);
    Surfaces::drawShape(context, {.bounds = math::Rect::fromCenter(tip, {size * 2.0F, size}), .rotation = pointing + kPi, .startAngle = -spread, .sweep = spread * 2.0F, .color = color});
}

} // namespace haylen::ui
