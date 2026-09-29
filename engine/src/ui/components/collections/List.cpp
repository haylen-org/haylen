#include "ui/components/collections/List.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/ChoiceItem.hpp"
#include "ui/components/DragAndDrop.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

void List::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
    reader.read("draggable", draggable);
}

// A list fills the width it gets, and an unbounded width, such as the one of a horizontal scroll, gets the width of its widest row.
math::Vec2 List::measureContent(Context& context, float availableWidth) {
    const float height = context.getMetric(Theme::Metric::ListRowHeight) * static_cast<float>(items.size());
    if (availableWidth < CommonProperties::kUnbounded) {
        return {availableWidth, height};
    }
    float widest = 0.0F;
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, ListRow::measure(context, item));
    }
    return {widest, height};
}

void List::render(Context& context, const math::Rect& bounds) {
    if (ImGui::GetDragDropPayload() == nullptr) {
        dragging.clear();
    }

    // The focus goes to the selected row, or to the first row that can take it.
    const bool focusing = takeFocusRequest();
    const auto target = std::ranges::find_if(items, [this](const ChoiceItem& item) { return item.id == selected && item.enabled; });
    const auto first = std::ranges::find_if(items, [](const ChoiceItem& item) { return item.enabled; });
    const std::string focused = !focusing ? std::string() : target != items.end() ? target->id : first != items.end() ? first->id : std::string();

    const float height = context.getMetric(Theme::Metric::ListRowHeight);
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const math::Rect area{bounds.x, bounds.y + height * static_cast<float>(index), bounds.width, height};
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = ListRow::draw(context, area, item.id == selected);
        const ImGuiID row = ImGui::GetItemID();
        const bool activated = GImGui->NavActivatePressedId == row;
        if (item.id == focused) {
            Widgets::focusItem(context);
        }
        ListRow::drawContent(context, area, item);
        const DragAndDrop::Result moved = draggable ? DragAndDrop::handle(context, *this, item.id, item.image, area) : DragAndDrop::Result{};
        if (moved.hovering) {
            ImGui::GetWindowDrawList()->AddRect(ImGuiConverter::toImVec2(area.getMin()), ImGuiConverter::toImVec2(area.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Accent)), context.getMetric(Theme::Metric::ControlRadius) * 0.5F, context.getMetric(Theme::Metric::FocusWidth));
        }
        DragAndDrop::drawCarried(context, row, area);
        ImGui::EndDisabled();
        ImGui::PopID();

        if (moved.dragged && dragging != item.id) {
            dragging = item.id;
            context.emit(*this, "drag", {{"item", item.id}});
        }
        if (moved.picked) {
            context.emit(*this, "drag", {{"item", item.id}});
        } else if (moved.dropped) {
            context.emit(*this, "drop", {{"item", item.id}, {"source", moved.dropped->first}, {"sourceItem", moved.dropped->second}});
        } else if (state.clicked && !(draggable && activated)) {
            selected = item.id;
            context.emit(*this, "select", {{"item", item.id}});
        }
    }
}

void List::drawingStopped(Context& context) {
    DragAndDrop::dropCarriedFrom(context, *this);
}

} // namespace haylen::ui
