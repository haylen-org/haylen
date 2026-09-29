#include "ui/Widgets.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>
#include <optional>

#include <imgui_internal.h>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Alignment.hpp"
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
    const Theme& theme = context.getTheme();
    if (state.held && theme.getSurface(set.pressed) != nullptr) {
        return theme.getSurface(set.pressed);
    }
    if (state.hovered && theme.getSurface(set.hover) != nullptr) {
        return theme.getSurface(set.hover);
    }
    return theme.getSurface(set.normal);
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

void Widgets::drawFocusRing(Context& context, const math::Rect& bounds, ImGuiID id, float radius) {
    if (!context.getFocus().isRingShown(id)) {
        return;
    }
    ImGuiWindow* window = GImGui->CurrentWindow;

    // The line runs a gap away from the edge of the control, and it may reach past the clip of a scrolled area so an item at the edge still shows it whole.
    const float width = context.getMetric(Theme::Metric::FocusWidth);
    const float distance = kFocusGap + width * 0.5F;
    ImRect ring = ImGuiConverter::toImRect(bounds);
    ring.ClipWith(window->ClipRect);
    ring.Expand(distance);
    const bool clipped = !window->ClipRect.Contains(ring);
    if (clipped) {
        window->DrawList->PushClipRect(ring.Min, ring.Max);
    }
    window->DrawList->AddRect(ring.Min, ring.Max, ImGuiConverter::toImU32(context.getColor(Theme::Color::Focus)), radius + distance, width);
    if (clipped) {
        window->DrawList->PopClipRect();
    }
}

void Widgets::focusItem(Context& context) {
    const ImRect& box = GImGui->LastItemData.Rect;
    context.getFocus().focus(ImGui::GetItemID(), math::Rect::fromMinMax({box.Min.x, box.Min.y}, {box.Max.x, box.Max.y}));
}

math::Vec2 Widgets::measureButton(Context& context, std::string_view label, bool hasIcon, ButtonVariant variant) {
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
    float width = textWidth + context.getMetric(Theme::Metric::ControlPaddingX) * 2.0F + padding.getHorizontal();
    if (hasIcon) {
        width += icon + (label.empty() ? 0.0F : kContentSpacing);
    }
    return {std::max(width, height), height};
}

bool Widgets::button(Context& context, const math::Rect& bounds, std::string_view label, const graphics::Texture* icon, ButtonVariant variant, bool checked) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius));

    if (variant == ButtonVariant::Link) {
        const math::Color color = context.getColor(state.hovered ? Theme::Color::AccentHover : Theme::Color::AccentText);
        Typography::drawAligned(context, Theme::Font::Button, bounds, color, label, Alignment::Start);
        if (state.hovered) {
            const float y = std::floor(bounds.y + (bounds.height + Typography::getLineHeight(context, Theme::Font::Button)) * 0.5F);
            ImGui::GetWindowDrawList()->AddLine({bounds.x, y}, {bounds.x + Typography::measure(context, Theme::Font::Button, label).x, y}, ImGuiConverter::toImU32(color), 1.0F);
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
        ImGui::GetWindowDrawList()->AddRectFilled(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Selection)), context.getMetric(Theme::Metric::ControlRadius));
    }

    // The label and icon sit together in the middle of the space the surface leaves free.
    const math::Rect inner = bounds.inset(Surfaces::getPadding(context, surfaces.normal));
    const float iconSize = icon != nullptr && icon->isValid() ? context.getMetric(Theme::Metric::IconSize) : 0.0F;
    const float labelWidth = label.empty() ? 0.0F : std::min(Typography::measure(context, Theme::Font::Button, label).x, std::max(0.0F, inner.width - iconSize - kContentSpacing));
    const float contentWidth = iconSize + labelWidth + (iconSize > 0.0F && labelWidth > 0.0F ? kContentSpacing : 0.0F);
    float x = std::floor(inner.x + (inner.width - contentWidth) * 0.5F);
    if (iconSize > 0.0F) {
        Surfaces::drawImage(context, *icon, {x, std::floor(inner.getCenter().y - iconSize * 0.5F), iconSize, iconSize}, variant == ButtonVariant::Icon || variant == ButtonVariant::Toolbar ? textColor : math::Color::white());
        x += iconSize + kContentSpacing;
    }
    if (labelWidth > 0.0F) {
        Typography::drawAligned(context, Theme::Font::Button, {x, inner.y, labelWidth, inner.height}, textColor, label, Alignment::Start);
    }
    return state.clicked;
}

math::Vec2 Widgets::measureChoice(Context& context, std::string_view label) {
    const float box = context.getMetric(Theme::Metric::ChoiceSize);
    const float width = box + (label.empty() ? 0.0F : kContentSpacing + Typography::measure(context, Theme::Font::Body, label).x);
    return {width, std::max(box, Typography::getLineHeight(context, Theme::Font::Body)) + context.getMetric(Theme::Metric::ControlPaddingY)};
}

bool Widgets::checkbox(Context& context, const math::Rect& bounds, bool& value, std::string_view label) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius));
    if (state.clicked) {
        value = !value;
    }

    const float size = context.getMetric(Theme::Metric::ChoiceSize);
    const math::Rect box{bounds.x, std::floor(bounds.getCenter().y - size * 0.5F), size, size};
    const Theme::Surface role = value ? Theme::Surface::CheckChecked : Theme::Surface::Check;
    math::Color fill = context.getColor(value ? Theme::Color::Accent : Theme::Color::Raised);
    if (state.hovered) {
        fill = mix(fill, context.getColor(Theme::Color::Hover));
    }
    Surfaces::draw(context, role, box, fill, context.getColor(value ? Theme::Color::Accent : Theme::Color::BorderStrong), size * 0.25F);
    if (value && context.getTheme().getSurface(role) == nullptr) {
        const float padding = size * 0.22F;
        ImGui::RenderCheckMark(ImGui::GetWindowDrawList(), {box.x + padding, box.y + padding}, ImGuiConverter::toImU32(context.getColor(Theme::Color::OnAccent)), size - padding * 2.0F);
    }
    if (!label.empty()) {
        Typography::drawAligned(context, Theme::Font::Body, {box.getRight() + kContentSpacing, bounds.y, bounds.getRight() - box.getRight() - kContentSpacing, bounds.height}, context.getColor(Theme::Color::Text), label, Alignment::Start);
    }
    return state.clicked;
}

bool Widgets::radio(Context& context, const math::Rect& bounds, bool selected, std::string_view label, std::string_view idLabel) {
    const Interaction state = interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius), idLabel);
    const float size = context.getMetric(Theme::Metric::ChoiceSize);
    const ImVec2 center{bounds.x + size * 0.5F, bounds.getCenter().y};
    math::Color fill = context.getColor(Theme::Color::Raised);
    if (state.hovered) {
        fill = mix(fill, context.getColor(Theme::Color::Hover));
    }
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddCircleFilled(center, size * 0.5F, ImGuiConverter::toImU32(fill));
    list.AddCircle(center, size * 0.5F, ImGuiConverter::toImU32(context.getColor(selected ? Theme::Color::Accent : Theme::Color::BorderStrong)), 0, context.getMetric(Theme::Metric::BorderWidth));
    if (selected) {
        list.AddCircleFilled(center, size * 0.25F, ImGuiConverter::toImU32(context.getColor(Theme::Color::Accent)));
    }
    if (!label.empty()) {
        Typography::drawAligned(context, Theme::Font::Body, {bounds.x + size + kContentSpacing, bounds.y, bounds.width - size - kContentSpacing, bounds.height}, context.getColor(Theme::Color::Text), label, Alignment::Start);
    }
    return state.clicked;
}

math::Vec2 Widgets::measureToggle(Context& context, std::string_view label) {
    const float width = context.getMetric(Theme::Metric::ToggleWidth) + (label.empty() ? 0.0F : kContentSpacing + Typography::measure(context, Theme::Font::Body, label).x);
    return {width, std::max(context.getMetric(Theme::Metric::ToggleHeight), Typography::getLineHeight(context, Theme::Font::Body)) + context.getMetric(Theme::Metric::ControlPaddingY)};
}

bool Widgets::toggle(Context& context, const math::Rect& bounds, bool& value, std::string_view label) {
    const Interaction state = interact(context, bounds, bounds.height * 0.5F);
    if (state.clicked) {
        value = !value;
    }

    // The knob slides toward its side a little every frame, and ImGui storage keeps where it was.
    float& position = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetItemID(), value ? 1.0F : 0.0F);
    const float target = value ? 1.0F : 0.0F;
    const float step = context.getDeltaSeconds() * kToggleSpeed;
    position = position < target ? std::min(target, position + step) : std::max(target, position - step);

    const float width = context.getMetric(Theme::Metric::ToggleWidth);
    const float height = context.getMetric(Theme::Metric::ToggleHeight);
    const math::Rect track{bounds.x, std::floor(bounds.getCenter().y - height * 0.5F), width, height};
    const math::Color off = context.getColor(Theme::Color::BorderStrong);
    const math::Color on = context.getColor(Theme::Color::Accent);
    const math::Color trackColor{off.r + (on.r - off.r) * position, off.g + (on.g - off.g) * position, off.b + (on.b - off.b) * position, off.a + (on.a - off.a) * position};
    Surfaces::draw(context, value ? Theme::Surface::TrackFill : Theme::Surface::Track, track, state.hovered ? mix(trackColor, context.getColor(Theme::Color::Hover)) : trackColor, std::nullopt, height * 0.5F);

    const float knob = height - 6.0F;
    const math::Rect knobRect{track.x + 3.0F + (width - knob - 6.0F) * position, track.y + 3.0F, knob, knob};
    Surfaces::draw(context, Theme::Surface::Knob, knobRect, context.getColor(Theme::Color::OnAccent), std::nullopt, knob * 0.5F);
    if (!label.empty()) {
        Typography::drawAligned(context, Theme::Font::Body, {track.getRight() + kContentSpacing, bounds.y, bounds.getRight() - track.getRight() - kContentSpacing, bounds.height}, context.getColor(Theme::Color::Text), label, Alignment::Start);
    }
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

    const float knob = context.getMetric(Theme::Metric::SliderKnobSize);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, knob);
    ImRect grab;
    const double before = value;
    ImGui::SliderBehavior(box, id, ImGuiDataType_Double, &value, &minimum, &maximum, "%.4f", ImGuiSliderFlags_NoInput, &grab);
    ImGui::PopStyleVar();
    if (step > 0.0) {
        value = std::clamp(minimum + std::round((value - minimum) / step) * step, minimum, maximum);
    }
    drawFocusRing(context, bounds, id, bounds.height * 0.5F);

    const float trackHeight = context.getMetric(Theme::Metric::SliderTrackHeight);
    const float span = maximum > minimum ? static_cast<float>((value - minimum) / (maximum - minimum)) : 0.0F;
    const math::Rect track{bounds.x + knob * 0.5F, std::floor(bounds.getCenter().y - trackHeight * 0.5F), bounds.width - knob, trackHeight};
    const math::Rect groove = track.inset(Surfaces::getPadding(context, Theme::Surface::Track));
    Surfaces::draw(context, Theme::Surface::Track, track, context.getColor(Theme::Color::BorderStrong), std::nullopt, trackHeight * 0.5F);
    Surfaces::draw(context, Theme::Surface::TrackFill, {groove.x, groove.y, groove.width * span, groove.height}, context.getColor(Theme::Color::Accent), std::nullopt, groove.height * 0.5F);
    const math::Rect knobRect{track.x + track.width * span - knob * 0.5F, std::floor(bounds.getCenter().y - knob * 0.5F), knob, knob};
    const bool active = state.ActiveId == id;
    Surfaces::draw(context, Theme::Surface::Knob, knobRect, active || hovered ? mix(context.getColor(Theme::Color::OnAccent), context.getColor(Theme::Color::Pressed)) : context.getColor(Theme::Color::OnAccent), context.getColor(Theme::Color::Accent), knob * 0.5F);

    const bool changed = value != before;
    if (changed) {
        ImGui::MarkItemEdited(id);
    }
    return changed;
}

void Widgets::progress(Context& context, const math::Rect& bounds, float value, Tone tone) {
    const float amount = std::clamp(value, 0.0F, 1.0F);
    Surfaces::draw(context, Theme::Surface::Track, bounds, context.getColor(Theme::Color::Border), std::nullopt, bounds.height * 0.5F);
    if (amount <= 0.0F) {
        return;
    }

    // The fill stays inside the padding of the track surface, so a framed bar image keeps its frame visible.
    const math::Rect groove = bounds.inset(Surfaces::getPadding(context, Theme::Surface::Track));
    const math::Rect filled{groove.x, groove.y, std::max(groove.width * amount, context.getTheme().getSurface(Theme::Surface::TrackFill) != nullptr ? 0.0F : groove.height), groove.height};
    Surfaces::draw(context, Theme::Surface::TrackFill, filled, context.getColor(getToneColors(tone == Tone::Neutral ? Tone::Accent : tone).fill), std::nullopt, groove.height * 0.5F);
}

void Widgets::spinner(Context& context, math::Vec2 center, float radius, math::Color color) {
    constexpr float kArc = std::numbers::pi_v<float> * 1.5F;
    const auto start = static_cast<float>(std::fmod(context.getTime() * 6.0, 2.0 * std::numbers::pi));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PathClear();
    list.PathArcTo(ImGuiConverter::toImVec2(center), radius, start, start + kArc, 32);
    list.PathStroke(ImGuiConverter::toImU32(color), std::max(2.0F, radius * 0.2F));
}

void Widgets::arrow(math::Vec2 center, float size, ImGuiDir direction, math::Color color) {
    const float half = size * 0.5F;
    const bool vertical = direction == ImGuiDir_Up || direction == ImGuiDir_Down;
    const float sign = direction == ImGuiDir_Down || direction == ImGuiDir_Right ? 1.0F : -1.0F;
    const ImVec2 tip = vertical ? ImVec2{center.x, center.y + half * sign} : ImVec2{center.x + half * sign, center.y};
    const ImVec2 first = vertical ? ImVec2{center.x - half, center.y - half * sign} : ImVec2{center.x - half * sign, center.y - half};
    const ImVec2 second = vertical ? ImVec2{center.x + half, center.y - half * sign} : ImVec2{center.x - half * sign, center.y + half};
    ImGui::GetWindowDrawList()->AddTriangleFilled(first, second, tip, ImGuiConverter::toImU32(color));
}

void Widgets::placePopup(const math::Rect& anchor, float width) {
    ImGui::SetNextWindowPos({anchor.x, anchor.getBottom() + 4.0F});
    ImGui::SetNextWindowSizeConstraints({std::max(width, anchor.width), 0.0F}, {FLT_MAX, FLT_MAX});
}

} // namespace haylen::ui
