#include "ui/components/choices/SegmentedControl.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void SegmentedControl::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
}

math::Vec2 SegmentedControl::measureContent(Context& context, float availableWidth) {
    float widest = 0.0F;
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, Typography::measure(context, Theme::Font::Button, context.getText(item.text)).x);
    }
    const float segment = widest + context.getMetric(Theme::Metric::TabPaddingX) * 2.0F;
    return {std::min(availableWidth, segment * static_cast<float>(items.size())), context.getMetric(Theme::Metric::ControlHeight)};
}

// Segments share the width equally while every label fits with the tab padding on both sides. With less room each segment takes its label and the padding and the rest is shared equally, and with less still the padding shrinks to the content spacing and the labels share the room left by their widths. Returns the padding the labels keep.
float SegmentedControl::layOut(Context& context, float width) {
    widths.clear();
    float labels = 0.0F;
    float widest = 0.0F;
    for (const ChoiceItem& item : items) {
        widths.push_back(Typography::measure(context, Theme::Font::Button, context.getText(item.text)).x);
        labels += widths.back();
        widest = std::max(widest, widths.back());
    }
    const auto count = static_cast<float>(items.size());
    const float tab = context.getMetric(Theme::Metric::TabPaddingX);
    if ((widest + tab * 2.0F) * count <= width) {
        std::ranges::fill(widths, width / count);
        return tab;
    }
    const float padding = std::max(context.getMetric(Theme::Metric::ContentSpacing), std::min(tab, (width - labels) / (count * 2.0F)));
    const float room = width - padding * 2.0F * count;
    const float share = room > labels ? (room - labels) / count : 0.0F;
    const float squeeze = room < labels && labels > 0.0F ? std::max(0.0F, room) / labels : 1.0F;
    for (float& segment : widths) {
        segment = segment * squeeze + share + padding * 2.0F;
    }
    return padding;
}

// A segment rounds only the corners it shares with the control, with the curve of the control inside its padding, so a picked segment never leaves empty corners in the control around it.
ImDrawFlags SegmentedControl::getCorners(const Context& context, std::size_t index) const noexcept {
    const bool first = index == 0;
    const bool last = index + 1 == items.size();
    const bool left = context.isRightToLeft() ? last : first;
    const bool right = context.isRightToLeft() ? first : last;
    if (left && right) {
        return ImDrawFlags_RoundCornersAll;
    }
    if (left) {
        return ImDrawFlags_RoundCornersLeft;
    }
    return right ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone;
}

void SegmentedControl::render(Context& context, const math::Rect& bounds) {
    const float outer = context.getMetric(Theme::Metric::ControlRadius);
    Surfaces::draw(context, Theme::Surface::Segment, bounds, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border), outer);
    if (items.empty()) {
        return;
    }

    // Segments share the width as `layOut` divides it, each picks itself on a click, and the whole control takes the focus on top of them. The segments sit inside the border of a flat control, or inside the padding of the image of a themed one, so a picked segment never covers the rim of the control.
    const int current = ChoiceItem::indexOf(items, selected);
    const math::Insets padding = context.getSurface(Theme::Surface::Segment) != nullptr ? Surfaces::getPadding(context, Theme::Surface::Segment) : math::Insets::uniform(context.getMetric(Theme::Metric::BorderWidth) * 2.0F);
    const math::Rect inner = bounds.inset(padding);
    const float radius = Surfaces::getInnerRadius(outer, std::max(padding.left, padding.top));
    const float labelPadding = layOut(context, inner.width);
    std::optional<int> clicked;
    float offset = 0.0F;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const float width = widths[index];
        const math::Rect segment = context.mirror({std::floor(inner.x + offset), inner.y, std::floor(inner.x + offset + width) - std::floor(inner.x + offset), inner.height}, inner);
        offset += width;
        const bool chosen = static_cast<int>(index) == current;
        const ImDrawFlags corners = getCorners(context, index);
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = Widgets::interact(context, segment, radius, "##segment", ImGuiButtonFlags_NoNavFocus);
        if (chosen) {
            Surfaces::draw(context, Theme::Surface::SegmentSelected, segment, context.getColor(Theme::Color::Accent), std::nullopt, radius, corners);
        } else if (state.hovered) {
            Surfaces::fill(context, segment, context.getColor(state.held ? Theme::Color::Pressed : Theme::Color::Hover), radius, corners);
        }
        if (index > 0 && !chosen && static_cast<int>(index) != current + 1) {
            const float thickness = context.getMetric(Theme::Metric::BorderWidth);
            const float edge = (context.isRightToLeft() ? segment.getRight() : segment.x) - thickness * 0.5F;
            Widgets::line(context, {edge, segment.y + segment.height * 0.25F}, {edge, segment.getBottom() - segment.height * 0.25F}, thickness, context.getColor(Theme::Color::Border));
        }
        Typography::drawAligned(context, Theme::Font::Button, segment.inset({labelPadding, 0.0F, labelPadding, 0.0F}), context.getColor(chosen ? Theme::Color::OnAccent : Theme::Color::Text), context.getText(item.text), Alignment::Center);
        ImGui::EndDisabled();
        ImGui::PopID();
        if (state.clicked) {
            clicked = static_cast<int>(index);
        }
    }

    const Widgets::Interaction body = Widgets::interact(context, bounds, outer, "##segments");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }

    if (clicked) {
        select(context, *clicked);
    } else if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        select(context, findNext(current, Widgets::getStep(context, *direction), false));
    } else if (body.clicked) {
        select(context, findNext(current, 1, true));
    }
}

// Without a selection the search starts just outside the end it moves away from, so it can reach every segment.
int SegmentedControl::findNext(int from, int direction, bool wrap) const {
    const auto count = static_cast<int>(items.size());
    const int origin = from >= 0 ? from : (direction > 0 ? -1 : count);
    const int steps = from >= 0 ? count - 1 : count;
    for (int step = 1; step <= steps; ++step) {
        const int index = origin + direction * step;
        if (!wrap && (index < 0 || index >= count)) {
            break;
        }
        if (items[static_cast<std::size_t>((index % count + count) % count)].enabled) {
            return (index % count + count) % count;
        }
    }
    return from;
}

void SegmentedControl::select(Context& context, int index) {
    if (index < 0 || items[static_cast<std::size_t>(index)].id == selected) {
        return;
    }
    selected = items[static_cast<std::size_t>(index)].id;
    context.emit(*this, "change", {{"value", selected}});
}

bool SegmentedControl::describe(Context& context, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::RadioGroup;
    if (const int index = ChoiceItem::indexOf(items, selected); index >= 0) {
        element.value.assign(context.getText(items[static_cast<std::size_t>(index)].text));
    }
    element.actions = platform::AccessibilityElement::Increment | platform::AccessibilityElement::Decrement;
    return true;
}

} // namespace haylen::ui
