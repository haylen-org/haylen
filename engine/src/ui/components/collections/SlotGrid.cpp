#include "ui/components/collections/SlotGrid.hpp"

#include <algorithm>
#include <set>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/JsonValidator.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/DragAndDrop.hpp"

namespace haylen::ui {

void SlotGrid::readProperties(PropertyReader& reader) {
    reader.read("columns", columns, 1, 64);
    reader.read("slotSize", slotSize, 0.0F, 1024.0F);
    if (reader.has("gap")) {
        float value = 0.0F;
        reader.read("gap", value, 0.0F, 1024.0F);
        gap = value;
    }
    if (const core::Json* parsed = reader.take("slots")) {
        slots = readSlots(reader, *parsed);
    }
    reader.read("selected", selected);
    reader.read("draggable", draggable);
}

std::vector<SlotGrid::Slot> SlotGrid::readSlots(PropertyReader& reader, const core::Json& value) {
    if (!PropertyReader::isList(value)) {
        reader.fail("slots", "must be a list");
    }
    std::vector<Slot> parsed;
    std::set<std::string, std::less<>> ids;
    for (const core::Json& entry : value) {
        if (!entry.is_object() || !entry.contains("id") || !entry.at("id").is_string() || entry.at("id").get<std::string>().empty()) {
            reader.fail("slots", "must hold objects with an id");
        }
        core::JsonValidator::requireKnownKeys(entry, {"id", "image", "count", "enabled"}, "\"slotGrid.slots\"");
        Slot slot{.id = entry.at("id").get<std::string>()};
        if (!ids.insert(slot.id).second) {
            reader.fail("slots", "uses the slot id \"" + slot.id + "\" more than once");
        }
        if (const auto image = entry.find("image"); image != entry.end()) {
            if (!image->is_string()) {
                reader.fail("slots", "has an image that is not a path");
            }
            slot.image = image->get<std::string>();
        }
        if (const auto count = entry.find("count"); count != entry.end()) {
            slot.count = TextValue::fromJson(*count, "slotGrid.slots.count");
        }
        if (const auto enabled = entry.find("enabled"); enabled != entry.end()) {
            if (!enabled->is_boolean()) {
                reader.fail("slots", "has an \"enabled\" that is not \"true\" or \"false\"");
            }
            slot.enabled = enabled->get<bool>();
        }
        parsed.push_back(std::move(slot));
    }
    return parsed;
}

float SlotGrid::getSlotSize(Context& context) const {
    return slotSize > 0.0F ? slotSize : context.getMetric(Theme::Metric::SlotSize);
}

float SlotGrid::getGap(Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing) * 0.5F);
}

math::Vec2 SlotGrid::measureContent(Context& context, float) {
    const float size = getSlotSize(context);
    const auto count = static_cast<int>(slots.size());
    const int across = std::min(columns, std::max(count, 1));
    const int rows = (count + columns - 1) / columns;
    return {size * static_cast<float>(across) + getGap(context) * static_cast<float>(across - 1), size * static_cast<float>(rows) + getGap(context) * static_cast<float>(std::max(rows - 1, 0))};
}

void SlotGrid::render(Context& context, const math::Rect& bounds) {
    if (ImGui::GetDragDropPayload() == nullptr) {
        dragging.clear();
    }

    // The focus goes to the selected slot, or to the first slot that can take it.
    const bool focusing = takeFocusRequest();
    const auto target = std::ranges::find_if(slots, [this](const Slot& slot) { return slot.id == selected && slot.enabled; });
    const auto first = std::ranges::find_if(slots, [](const Slot& slot) { return slot.enabled; });
    const std::string focused = !focusing ? std::string() : target != slots.end() ? target->id : first != slots.end() ? first->id : std::string();

    const float size = getSlotSize(context);
    const float spacing = getGap(context);
    for (std::size_t index = 0; index < slots.size(); ++index) {
        const auto column = static_cast<float>(index % static_cast<std::size_t>(columns));
        const auto row = static_cast<float>(index / static_cast<std::size_t>(columns));
        const math::Rect area = context.mirror({bounds.x + (size + spacing) * column, bounds.y + (size + spacing) * row, size, size}, bounds);
        drawSlot(context, slots[index], area, !focused.empty() && slots[index].id == focused);
    }
}

void SlotGrid::drawSlot(Context& context, const Slot& slot, const math::Rect& area, bool focusTarget) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    ImGui::PushID(slot.id.c_str());
    ImGui::BeginDisabled(!slot.enabled);
    const Widgets::Interaction state = Widgets::interact(context, area, radius, "##slot");
    const ImGuiID item = ImGui::GetItemID();
    const bool activated = GImGui->NavActivatePressedId == item;
    if (focusTarget) {
        Widgets::focusItem(context);
    }

    if (Widgets::isVisible(context, area)) {
        drawContent(context, slot, area, state.hovered);
    }

    const DragAndDrop::Result moved = draggable ? DragAndDrop::handle(context, *this, slot.id, slot.image, area) : DragAndDrop::Result{};
    if (moved.hovering) {
        ImGui::GetWindowDrawList()->AddRect(ImGuiConverter::toImVec2(area.getMin()), ImGuiConverter::toImVec2(area.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::Accent)), radius, context.getMetric(Theme::Metric::FocusWidth));
    }
    DragAndDrop::drawCarried(context, item, area);
    ImGui::EndDisabled();
    ImGui::PopID();

    if (moved.dragged && dragging != slot.id) {
        dragging = slot.id;
        context.emit(*this, "dragStart", {{"item", slot.id}});
    }
    if (moved.picked) {
        context.emit(*this, "dragStart", {{"item", slot.id}});
    } else if (moved.dropped) {
        context.emit(*this, "drop", {{"item", slot.id}, {"source", moved.dropped->first}, {"sourceItem", moved.dropped->second}});
    } else if (state.clicked && !(draggable && activated)) {
        selected = slot.id;
        context.emit(*this, "select", {{"item", slot.id}});
    }
}

void SlotGrid::drawContent(Context& context, const Slot& slot, const math::Rect& area, bool hovered) const {
    const bool chosen = slot.id == selected;
    const Theme::Surface surface = chosen || hovered ? Theme::Surface::SlotHighlighted : Theme::Surface::Slot;
    Surfaces::draw(context, surface, area, context.getColor(chosen ? Theme::Color::Selection : hovered ? Theme::Color::BorderStrong : Theme::Color::Raised), context.getColor(chosen ? Theme::Color::Accent : Theme::Color::Border), context.getMetric(Theme::Metric::ControlRadius));
    const bool leaving = dragging == slot.id || (context.getFocus().getCarried() && context.getFocus().getCarried()->source == getId() && context.getFocus().getCarried()->item == slot.id);
    if (!slot.image.empty()) {
        const float inset = area.width * 0.12F;
        Surfaces::drawImage(context, context.getImage(slot.image), area.expanded(-inset), math::Color::white().withAlpha(leaving ? 0.35F : 1.0F));
    }
    if (const std::string count = context.getText(slot.count); !count.empty()) {
        const float line = Typography::getLineHeight(context, Theme::Font::Caption);
        Typography::drawParagraph(context, Theme::Font::Caption, {area.x, area.getBottom() - line - 4.0F, area.width - 8.0F, line}, context.getColor(Theme::Color::Text), count, Alignment::End, context.getColor(Theme::Color::Window), 2.0F);
    }
}

void SlotGrid::drawingStopped(Context& context) {
    DragAndDrop::dropCarriedFrom(context, *this);
}

} // namespace haylen::ui
