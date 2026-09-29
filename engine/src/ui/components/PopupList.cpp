#include "ui/components/PopupList.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

std::optional<std::string> PopupList::draw(Context& context, std::string_view popup, const std::vector<ChoiceItem>& items, std::string_view current, float minimumWidth) {
    const std::string name(popup);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    const bool open = ImGui::BeginPopup(name.c_str(), ImGuiWindowFlags_NoNavInputs);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (!open) {
        return std::nullopt;
    }

    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    Surfaces::draw(context, Theme::Surface::Menu, {window.Pos.x, window.Pos.y, window.Size.x, window.Size.y}, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border));

    const float height = context.getMetric(Theme::Metric::ListRowHeight) * 0.75F;
    const float width = std::max(minimumWidth, measureWidth(context, items));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    std::optional<std::string> picked;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const math::Rect row{origin.x, origin.y + height * static_cast<float>(index), width, height};
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = ListRow::draw(context, row, item.id == current);
        if (item.id == current && ImGui::IsWindowAppearing()) {
            Widgets::focusItem(context);
        }
        ListRow::drawContent(context, row, item);
        ImGui::EndDisabled();
        ImGui::PopID();
        if (state.clicked) {
            picked = item.id;
        }
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({width, height * static_cast<float>(items.size())});
    if (picked) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    return picked;
}

float PopupList::measureWidth(Context& context, const std::vector<ChoiceItem>& items) {
    float widest = 0.0F;
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, ListRow::measure(context, item));
    }
    return widest;
}

} // namespace haylen::ui
