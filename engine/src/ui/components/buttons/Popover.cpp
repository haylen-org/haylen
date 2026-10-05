#include "ui/components/buttons/Popover.hpp"

#include <algorithm>
#include <optional>

#include <imgui.h>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Popover::readMore(PropertyReader& reader) {
    reader.read("contentWidth", contentWidth, 0.0F, 10000.0F);
}

void Popover::render(Context& context, const math::Rect& bounds) {
    if (drawButton(context, bounds)) {
        ImGui::OpenPopup("##popover");
    }
    if (!ImGui::IsPopupOpen("##popover")) {
        return;
    }
    Component* content = getChildren().empty() ? nullptr : getChildren().front().get();
    const math::Vec2 size = content != nullptr ? content->measure(context, contentWidth) : math::Vec2{};
    if (const std::optional<math::Rect> box = popup.begin(context, "##popover", ImGuiWindowFlags_NoNavInputs, context.getMetric(Theme::Metric::PanelPadding), {std::max(size.x, contentWidth), size.y}, bounds, contentWidth)) {
        if (content != nullptr) {
            content->draw(context, *box);
        }
        popup.end(context);
    }
}

} // namespace haylen::ui
