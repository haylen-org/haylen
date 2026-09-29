#include "ui/components/DragAndDrop.hpp"

#include <algorithm>

#include <imgui_internal.h>

#include "haylen/core/Json.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

DragAndDrop::Result DragAndDrop::handle(Context& context, const Component& component, std::string_view entry, std::string_view image, const math::Rect& bounds) {
    Result result;
    const ImGuiContext& state = *GImGui;
    const ImGuiID item = state.LastItemData.ID;

    // A drag carries the node and the entry it left, and shows its picture under the pointer.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        const std::string payload = core::Json{{"source", component.getId()}, {"item", entry}}.dump();
        ImGui::SetDragDropPayload(kPayload, payload.data(), payload.size());
        const float side = std::min(bounds.width, bounds.height);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (!image.empty()) {
            Surfaces::drawImage(context, context.getImage(image), {origin.x, origin.y, side, side});
        }
        ImGui::Dummy({side, side});
        ImGui::EndDragDropSource();
        result.dragged = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            result.hovering = true;
            if (payload->IsDelivery()) {
                const core::Json dropped = core::Json::parse(std::string_view(static_cast<const char*>(payload->Data), static_cast<std::size_t>(payload->DataSize)));
                result.dropped = std::pair{dropped.at("source").get<std::string>(), dropped.at("item").get<std::string>()};
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Accept picks the focused entry up, and accept on another entry drops it there. Accept on the carried entry puts it back.
    FocusNavigator& focus = context.getFocus();
    const std::optional<FocusNavigator::Carry>& carried = focus.getCarried();
    result.hovering = result.hovering || (carried && state.NavId == item);
    if (item == 0 || state.NavActivatePressedId != item) {
        return result;
    }
    if (!carried) {
        focus.carry({.source = component.getId(), .item = std::string(entry), .image = std::string(image)});
        result.picked = true;
        return result;
    }
    if (carried->source != component.getId() || carried->item != entry) {
        result.dropped = std::pair{carried->source, carried->item};
    }
    focus.dropCarried();
    return result;
}

void DragAndDrop::drawCarried(Context& context, ImGuiID item, const math::Rect& bounds) {
    const std::optional<FocusNavigator::Carry>& carried = context.getFocus().getCarried();
    if (!carried || carried->image.empty() || GImGui->NavId != item) {
        return;
    }
    const float side = std::min(bounds.width, bounds.height) * 0.6F;
    Surfaces::drawImage(context, context.getImage(carried->image), {bounds.getRight() - side * 0.75F, bounds.y - side * 0.25F, side, side}, math::Color::white().withAlpha(0.85F));
}

} // namespace haylen::ui
