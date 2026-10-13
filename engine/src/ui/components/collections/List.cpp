#include "ui/components/collections/List.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/AccessibilityTree.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
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

// Only the rows in view, one row on each side of them and the focused row draw and register, so a long list costs what its view shows.
void List::render(Context& context, const math::Rect& bounds) {
    if (ImGui::GetDragDropPayload() == nullptr) {
        dragging.clear();
    }

    // The focus goes to the selected row, or to the first row that can take it.
    const bool focusing = takeFocusRequest();
    const auto target = std::ranges::find_if(items, [this](const ChoiceItem& item) { return item.id == selected && item.enabled; });
    const auto first = std::ranges::find_if(items, [](const ChoiceItem& item) { return item.enabled; });
    const std::string_view focused = !focusing ? std::string_view() : target != items.end() ? std::string_view(target->id) : first != items.end() ? std::string_view(first->id) : std::string_view();
    const auto focusedIndex = focused.empty() ? items.end() : std::ranges::find(items, focused, &ChoiceItem::id);

    const float height = context.getMetric(Theme::Metric::ListRowHeight);
    const auto [from, to] = findRowRange(context, bounds, height);
    const auto extra = focusedIndex != items.end() ? std::optional(static_cast<std::size_t>(focusedIndex - items.begin())) : remembered;
    remembered.reset();
    if (extra && *extra < items.size() && (*extra < from || *extra >= to)) {
        drawRow(context, bounds, height, *extra, focused);
    }
    for (std::size_t index = from; index < to; ++index) {
        drawRow(context, bounds, height, index, focused);
    }
}

// The rows of the visible part of the clip of the window, in the units of the list, which the transforms around it may scale.
std::pair<std::size_t, std::size_t> List::findRowRange(const Context& context, const math::Rect& bounds, float height) const {
    const Context::Reshape shape = context.getReshape();
    if (height <= 0.0F || shape.scale.x == 0.0F || shape.scale.y == 0.0F) {
        return {0, 0};
    }
    const math::Rect clip = Context::Reshape{.scale = math::Vec2{1.0F, 1.0F} / shape.scale, .offset = math::Vec2{} - shape.offset / shape.scale}.apply(ImGuiConverter::toRect(GImGui->CurrentWindow->ClipRect));
    const float top = std::floor((clip.y - bounds.y) / height) - 1.0F;
    const float bottom = std::ceil((clip.getBottom() - bounds.y) / height) + 1.0F;
    const auto limit = static_cast<float>(items.size());
    return {static_cast<std::size_t>(std::clamp(top, 0.0F, limit)), static_cast<std::size_t>(std::clamp(bottom, 0.0F, limit))};
}

// Accept on a row of a draggable list chooses it first, and accept on the chosen row picks it up, so the keyboard, gamepads and remotes both choose rows and carry them.
void List::drawRow(Context& context, const math::Rect& bounds, float height, std::size_t index, std::string_view focused) {
    const ChoiceItem& item = items[index];
    const math::Rect area{bounds.x, bounds.y + height * static_cast<float>(index), bounds.width, height};
    ImGui::PushID(item.id.c_str());
    ImGui::BeginDisabled(!item.enabled);
    const Widgets::Interaction state = ListRow::draw(context, area, item.id == selected);
    const ImGuiID row = ImGui::GetItemID();
    context.getAccessibility().addPart(row, area, platform::AccessibilityElement::Role::Button, context.getText(item.text), (item.id == selected ? platform::AccessibilityElement::Selected : 0) | (item.enabled ? 0 : platform::AccessibilityElement::Disabled));
    const bool activated = GImGui->NavActivatePressedId == row;
    const bool choosing = draggable && activated && item.id != selected && !context.getFocus().getCarried();
    if (item.id == focused) {
        Widgets::focusItem(context);
    }
    if (GImGui->NavId == row) {
        remembered = index;
    }
    ListRow::drawContent(context, area, item);
    const DragAndDrop::Result moved = draggable ? DragAndDrop::handle(context, *this, item.id, item.image, area, !choosing) : DragAndDrop::Result{};
    if (moved.hovering) {
        Surfaces::outline(context, area, context.getColor(Theme::Color::Accent), context.getMetric(Theme::Metric::ControlRadius) * 0.5F, context.getMetric(Theme::Metric::FocusWidth));
    }
    DragAndDrop::drawCarried(context, row, area);
    ImGui::EndDisabled();
    ImGui::PopID();

    if (moved.dragged && dragging != item.id) {
        dragging = item.id;
        context.emit(*this, "dragStart", {{"item", item.id}});
    }
    if (moved.picked) {
        context.emit(*this, "dragStart", {{"item", item.id}});
    } else if (moved.dropped) {
        context.emit(*this, "drop", {{"item", item.id}, {"source", moved.dropped->first}, {"sourceItem", moved.dropped->second}});
    } else if (state.clicked && (!draggable || !activated || choosing)) {
        selected = item.id;
        context.emit(*this, "select", {{"item", item.id}});
    }
}

void List::drawingStopped(Context& context) {
    DragAndDrop::dropCarriedFrom(context, *this);
}

bool List::describe(Context&, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::List;
    return true;
}

} // namespace haylen::ui
