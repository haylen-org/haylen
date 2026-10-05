#include "ui/components/buttons/Popover.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Popover::readMore(PropertyReader& reader) {
    reader.read("contentWidth", contentWidth, 0.0F, 10000.0F);
}

void Popover::render(Context& context, const math::Rect& bounds) {
    if (drawButton(context, bounds)) {
        ImGui::OpenPopup("##popover");
    }
    Widgets::placePopup(context, bounds, contentWidth);
    if (Widgets::beginPopup(context, "##popover", ImGuiWindowFlags_NoNavInputs, context.getMetric(Theme::Metric::PanelPadding))) {
        if (!getChildren().empty()) {
            Component& content = *getChildren().front();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const math::Vec2 size = content.measure(context, contentWidth);
            content.draw(context, {origin.x, origin.y, std::max(size.x, contentWidth), size.y});
            ImGui::Dummy({std::max(size.x, contentWidth), size.y});
        }
        ImGui::EndPopup();
    }
}

} // namespace haylen::ui
