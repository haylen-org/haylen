#include "ui/Popup.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <imgui_internal.h>

#include "haylen/math/Insets.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"

namespace haylen::ui {

std::optional<math::Rect> Popup::begin(Context& context, const char* name, ImGuiWindowFlags flags, float padding, math::Vec2 content, const math::Rect& anchor, float minimumWidth) {
    // A popup is as tall as its content up to the room on its side of the anchor, inside the margins ImGui keeps around popups, and the content of a taller one scrolls, which is known before the popup opens, so its size never changes back and forth.
    const bool rightToLeft = context.isRightToLeft();
    const math::Rect display = context.getBackend().getDisplayRect();
    const float margin = ImGui::GetStyle().DisplaySafeAreaPadding.y;
    const float needed = content.y + padding * 2.0F;
    const float roomBelow = display.getBottom() - margin - anchor.getBottom() - kAnchorGap;
    const float roomAbove = anchor.y - kAnchorGap - display.y - margin;
    const bool below = needed <= roomBelow || roomBelow >= roomAbove;
    const float limit = std::max(0.0F, below ? roomBelow : roomAbove);
    const math::Insets edge = Surfaces::getFrame(context, Theme::Surface::Menu);
    overflowing = needed > limit;
    const float reserve = overflowing ? Scrollbar::getReserve(context, padding - (rightToLeft ? edge.left : edge.right)) : 0.0F;
    ImGui::SetNextWindowPos({rightToLeft ? anchor.getRight() : anchor.x, below ? anchor.getBottom() + kAnchorGap : anchor.y - kAnchorGap}, ImGuiCond_Always, {rightToLeft ? 1.0F : 0.0F, below ? 0.0F : 1.0F});
    ImGui::SetNextWindowSizeConstraints({std::max(minimumWidth, anchor.width), 0.0F}, {FLT_MAX, limit});
    ImGui::SetNextWindowContentSize({std::ceil(content.x + reserve), content.y});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {padding, padding});
    const bool open = ImGui::BeginPopup(name, flags | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (!open) {
        return std::nullopt;
    }

    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    const math::Rect frame{window.Pos.x, window.Pos.y, window.Size.x, window.Size.y};
    const float radius = context.getMetric(Theme::Metric::PanelRadius);
    Surfaces::drawShadow(context, frame, radius);
    Surfaces::draw(context, Theme::Surface::Menu, frame, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border), radius);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    box = {start.x + (rightToLeft ? reserve : 0.0F), start.y, content.x, content.y};

    // The bar runs inside the frame clear of its rounded corners, and the content draws only outside the lane of the bar, which ImGui may narrow when it rounds the size of the popup down to whole units.
    if (overflowing) {
        const math::Rect area = frame.inset(edge);
        box = Scrollbar::getContentBox(context, area, box, false);
        if (const std::optional<double> offset = scrollbar.interact(context, area, false, false, ImGui::GetScrollY(), ImGui::GetScrollMaxY(), frame.height, Surfaces::getInnerRadius(radius, rightToLeft ? edge.left : edge.right))) {
            ImGui::SetScrollY(static_cast<float>(*offset));
        }
        const math::Rect clip = Scrollbar::getContentBox(context, area, area, false);
        ImGui::PushClipRect(ImGuiConverter::toImVec2(clip.getMin()), ImGuiConverter::toImVec2(clip.getMax()), true);
    }
    return box;
}

void Popup::end(Context& context) {
    if (overflowing) {
        ImGui::PopClipRect();
        scrollbar.draw(context, 1.0F);
    }
    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(box.getMin()));
    ImGui::Dummy(ImGuiConverter::toImVec2(box.getSize()));
    ImGui::EndPopup();
}

} // namespace haylen::ui
