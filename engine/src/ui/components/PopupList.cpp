#include "ui/components/PopupList.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/Popup.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

// The menu keeps the theme menu padding around its rows, so with the width of its anchor it lines up with it, and the rows round their corners in parallel with the corners of the menu.
std::optional<std::string> PopupList::draw(Context& context, Popup& popup, std::string_view name, const std::vector<ChoiceItem>& items, std::string_view current, const math::Rect& anchor, float minimumWidth) {
    const std::string id(name);
    if (!ImGui::IsPopupOpen(id.c_str())) {
        return std::nullopt;
    }
    const float padding = context.getMetric(Theme::Metric::MenuPadding);
    const float height = context.getMetric(Theme::Metric::ListRowHeight) * 0.75F;
    const float width = std::max(minimumWidth - padding * 2.0F, measureWidth(context, items));
    const std::optional<math::Rect> box = popup.begin(context, id.c_str(), ImGuiWindowFlags_NoNavInputs, padding, {width, height * static_cast<float>(items.size())}, anchor, minimumWidth);
    if (!box) {
        return std::nullopt;
    }

    const float rowRadius = Surfaces::getInnerRadius(context.getMetric(Theme::Metric::PanelRadius), padding);
    std::optional<std::string> picked;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const math::Rect row{box->x, box->y + height * static_cast<float>(index), box->width, height};
        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = ListRow::draw(context, row, item.id == current, rowRadius);
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
    if (picked) {
        ImGui::CloseCurrentPopup();
    }
    popup.end(context);
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
