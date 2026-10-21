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

// The control is as wide as its segments with every label at full size, and grows taller when its labels wrap in less room.
math::Vec2 SegmentedControl::measureContent(Context& context, float availableWidth) {
    float widest = 0.0F;
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, Typography::measure(context, Theme::Font::Button, context.getText(item.text)).x);
    }
    const math::Insets padding = getPadding(context);
    const float segment = widest + context.getMetric(Theme::Metric::TabPaddingX) * 2.0F;
    const float width = std::min(availableWidth, segment * static_cast<float>(items.size()) + padding.getHorizontal());
    const float labelPadding = layOut(context, std::max(0.0F, width - padding.getHorizontal()));
    float tallest = 0.0F;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const float room = std::max(1.0F, widths[index] - labelPadding * 2.0F);
        tallest = std::max(tallest, Typography::layout(context, Theme::Font::Button, context.getText(items[index].text), getLabelStyle(context, room))->size.y);
    }
    const float height = std::max(context.getMetric(Theme::Metric::ControlHeight), tallest + context.getMetric(Theme::Metric::ControlPaddingY) + padding.getVertical());
    return {width, height};
}

// The segments sit inside the border of a flat control, or inside the padding of the image of a themed one.
math::Insets SegmentedControl::getPadding(Context& context) const {
    if (context.getSurface(Theme::Surface::Segment) != nullptr) {
        return Surfaces::getPadding(context, Theme::Surface::Segment);
    }
    return math::Insets::uniform(context.getMetric(Theme::Metric::BorderWidth) * 2.0F);
}

// Segments share the width equally while every label fits with the tab padding on both sides. With less room each segment takes its label and the padding and the rest is shared equally, and with less still the padding shrinks to the content spacing and the labels draw smaller, down to the least scale. Below it the labels wrap between their words at that scale, or smaller when their longest words do not fit side by side, and each segment takes its longest word and a share of the room left in proportion to the rest of its label, so no word ever breaks. Returns the padding the labels keep.
float SegmentedControl::layOut(Context& context, float width) {
    widths.clear();
    labelScale = 1.0F;
    labelsWrap = false;
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
    const float room = std::max(0.0F, width - padding * 2.0F * count);
    if (labels <= room) {
        for (float& segment : widths) {
            segment += (room - labels) / count + padding * 2.0F;
        }
        return padding;
    }
    if (room >= labels * kMinLabelScale) {
        labelScale = room / labels;
        for (float& segment : widths) {
            segment = segment * labelScale + padding * 2.0F;
        }
        return padding;
    }

    words.clear();
    float longest = 0.0F;
    for (const ChoiceItem& item : items) {
        words.push_back(getLongestWord(context, context.getText(item.text)));
        longest += words.back();
    }
    labelScale = longest > 0.0F ? std::min(kMinLabelScale, room / longest) : kMinLabelScale;
    labelsWrap = true;
    const float spare = std::max(0.0F, room - longest * labelScale);
    const float rest = labels - longest;
    for (std::size_t index = 0; index < widths.size(); ++index) {
        const float share = rest > 0.0F ? (widths[index] - words[index]) / rest : 1.0F / count;
        widths[index] = words[index] * labelScale + spare * share + padding * 2.0F;
    }
    return padding;
}

// The longest run of a label between its spaces at the full size of its font, which its segment keeps room for so the word never breaks.
float SegmentedControl::getLongestWord(Context& context, std::string_view label) {
    float longest = 0.0F;
    while (!label.empty()) {
        const std::size_t end = label.find(' ');
        longest = std::max(longest, Typography::measure(context, Theme::Font::Button, label.substr(0, end)).x);
        label = end == std::string_view::npos ? std::string_view() : label.substr(end + 1);
    }
    return longest + 1.0F;
}

// A label takes the scale of the layout and wraps in the room of its segment only once the labels are at their least scale, so a label that fits never wraps on the rounding of its width.
text::Style SegmentedControl::getLabelStyle(Context& context, float width) const {
    text::Style style = Typography::getStyle(context, Theme::Font::Button, labelsWrap ? width : -1.0F, labelsWrap ? text::Alignment::Center : text::Alignment::Start);
    style.size *= labelScale;
    return style;
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
    const math::Insets padding = getPadding(context);
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
        drawLabel(context, segment.inset({labelPadding, 0.0F, labelPadding, 0.0F}), context.getText(item.text), context.getColor(chosen ? Theme::Color::OnAccent : Theme::Color::Text));
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

// Wrapped lines center themselves in the room of the label, and a single line centers as a whole.
void SegmentedControl::drawLabel(Context& context, const math::Rect& area, std::string_view label, math::Color color) const {
    text::Style style = getLabelStyle(context, std::max(1.0F, area.width));
    style.color = color;
    const math::Vec2 size = Typography::layout(context, Theme::Font::Button, label, style)->size;
    const float x = style.maxWidth > 0.0F ? area.x : area.x + (area.width - size.x) * 0.5F;
    Typography::drawLayout(context, Theme::Font::Button, label, style, {x, area.y + (area.height - size.y) * 0.5F});
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
