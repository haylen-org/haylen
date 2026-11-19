#include "ui/Popup.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <imgui_internal.h>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ScrollFade.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

std::optional<math::Rect> Popup::begin(Context& context, const char* name, ImGuiWindowFlags flags, float padding, math::Vec2 content, const math::Rect& anchor, float minimumWidth) {
    // A popup is as tall as its content up to the room on its side of the anchor, inside the margins ImGui keeps around popups, and the content of a taller one scrolls, which is known before the popup opens, so its size never changes back and forth.
    const bool rightToLeft = context.isRightToLeft();
    const math::Rect display = context.getBackend().getLiftedDisplayRect();
    const float margin = ImGui::GetStyle().DisplaySafeAreaPadding.y;
    const float needed = content.y + padding * 2.0F;
    const float roomBelow = display.getBottom() - margin - anchor.getBottom() - kAnchorGap;
    const float roomAbove = anchor.y - kAnchorGap - display.y - margin;
    const bool below = needed <= roomBelow || roomBelow >= roomAbove;
    const float limit = std::max(0.0F, below ? roomBelow : roomAbove);

    overflowing = needed > limit;

    const ImVec2 position{rightToLeft ? anchor.getRight() : anchor.x, below ? anchor.getBottom() + kAnchorGap : anchor.y - kAnchorGap};
    const ImVec2 pivot{rightToLeft ? 1.0F : 0.0F, below ? 0.0F : 1.0F};
    ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowSizeConstraints({std::max(minimumWidth, anchor.width), 0.0F}, {FLT_MAX, limit});
    ImGui::SetNextWindowContentSize({std::ceil(content.x), content.y});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {padding, padding});
    const bool open = ImGui::BeginPopup(name, flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (!open) {
        return std::nullopt;
    }

    context.pushPlainReshape();

    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    const math::Rect frame{window.Pos.x, window.Pos.y, window.Size.x, window.Size.y};
    const float radius = context.getMetric(Theme::Metric::PanelRadius);
    Surfaces::drawShadow(context, frame, radius);
    Surfaces::draw(context, Theme::Surface::Menu, frame, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border), radius);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    box = {start.x, start.y, content.x, content.y};

    // The rows fade out along the edges of the frame where more of them continue, as the content of the other areas that scroll does.
    const double shown = ImGui::GetScrollY();
    faded = overflowing && ScrollFade::begin(context, frame.inset(Surfaces::getFrame(context, Theme::Surface::Menu)), false, shown, ImGui::GetScrollMaxY() - shown);
    return box;
}

void Popup::end(Context& context) {
    if (faded) {
        ScrollFade::end(context);
    }

    if (overflowing) {
        scroll.updateWindow(context);
    }

    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(box.getMin()));
    ImGui::Dummy(ImGuiConverter::toImVec2(box.getSize()));
    context.popReshape();
    ImGui::EndPopup();
}

} // namespace haylen::ui
