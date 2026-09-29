#include "ui/components/choices/SegmentedControl.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
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

void SegmentedControl::render(Context& context, const math::Rect& bounds) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    Surfaces::draw(context, Theme::Surface::Segment, bounds, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border));
    if (items.empty()) {
        return;
    }

    // Segments share the width, each picks itself on a click, and the whole control takes the focus on top of them.
    const int current = ChoiceItem::indexOf(items, selected);
    const math::Rect inner = bounds.inset(Surfaces::getPadding(context, Theme::Surface::Segment));
    std::optional<int> clicked;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const float width = inner.width / static_cast<float>(items.size());
        const math::Rect segment = context.mirror({std::floor(inner.x + width * static_cast<float>(index)), inner.y, width, inner.height}, inner);
        const bool chosen = static_cast<int>(index) == current;
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = Widgets::interact(context, segment, radius, "##segment", ImGuiButtonFlags_NoNavFocus);
        if (chosen) {
            Surfaces::draw(context, Theme::Surface::SegmentSelected, segment, context.getColor(Theme::Color::Accent), std::nullopt, radius);
        } else if (state.hovered) {
            ImGui::GetWindowDrawList()->AddRectFilled(ImGuiConverter::toImVec2(segment.getMin()), ImGuiConverter::toImVec2(segment.getMax()), ImGuiConverter::toImU32(context.getColor(state.held ? Theme::Color::Pressed : Theme::Color::Hover)), radius);
        }
        if (index > 0 && !chosen && static_cast<int>(index) != current + 1) {
            const float edge = context.isRightToLeft() ? segment.getRight() : segment.x;
            ImGui::GetWindowDrawList()->AddLine({edge, segment.y + segment.height * 0.25F}, {edge, segment.getBottom() - segment.height * 0.25F}, ImGuiConverter::toImU32(context.getColor(Theme::Color::Border)), context.getMetric(Theme::Metric::BorderWidth));
        }
        Typography::drawAligned(context, Theme::Font::Button, segment, context.getColor(chosen ? Theme::Color::OnAccent : Theme::Color::Text), context.getText(item.text), Alignment::Center);
        ImGui::EndDisabled();
        ImGui::PopID();
        if (state.clicked) {
            clicked = static_cast<int>(index);
        }
    }

    const Widgets::Interaction body = Widgets::interact(context, bounds, radius, "##segments");
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

int SegmentedControl::findNext(int from, int direction, bool wrap) const {
    const auto count = static_cast<int>(items.size());
    for (int step = 1; step < count; ++step) {
        const int index = from + direction * step;
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

} // namespace haylen::ui
