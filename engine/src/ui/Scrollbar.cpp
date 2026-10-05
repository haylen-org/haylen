#include "ui/Scrollbar.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

std::optional<double> Scrollbar::interact(const math::Rect& area, bool horizontal, bool reversed, double offset, double maximum, float viewLength) {
    horizontalBar = horizontal;
    const float trackLength = horizontal ? area.width : area.height;
    const auto length = static_cast<float>(std::clamp(static_cast<double>(trackLength) * viewLength / (maximum + viewLength), static_cast<double>(std::min(kMinThumb, trackLength)), static_cast<double>(trackLength)));
    const float travel = trackLength - length;
    const float progress = maximum > 0.0 ? static_cast<float>(std::clamp(offset / maximum, 0.0, 1.0)) : 0.0F;
    const float start = (reversed ? 1.0F - progress : progress) * travel;
    thumb = horizontal ? math::Rect{area.x + start, area.y + kInset, length, area.height - kInset * 2.0F} : math::Rect{area.x + kInset, area.y + start, area.width - kInset * 2.0F, length};

    const ImGuiID id = ImGui::GetID("##scrollbar");
    const ImRect box = ImGuiConverter::toImRect(area);
    if (!ImGui::ItemAdd(box, id)) {
        hovered = held = grabbing = false;
        return std::nullopt;
    }
    const bool pressed = ImGui::ButtonBehavior(box, id, &hovered, &held, ImGuiButtonFlags_NoNavFocus | ImGuiButtonFlags_PressedOnClick);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float along = (horizontal ? mouse.x - area.x : mouse.y - area.y);

    // A press on the thumb grabs it where it was pressed, and a press on the track moves by one view toward the press.
    if (pressed) {
        grabbing = along >= start && along <= start + length;
        grab = along - start;
        if (!grabbing) {
            const bool backward = (along < start) != reversed;
            return std::clamp(offset + (backward ? -viewLength : viewLength), 0.0, maximum);
        }
    }
    if (!held) {
        grabbing = false;
    }
    if (!grabbing || travel <= 0.0F) {
        return std::nullopt;
    }
    const double ratio = std::clamp(static_cast<double>((along - grab) / travel), 0.0, 1.0);
    return (reversed ? 1.0 - ratio : ratio) * maximum;
}

void Scrollbar::draw(Context& context, float alpha) const {
    const math::Color color = context.getColor(hovered || held ? Theme::Color::ScrollbarHover : Theme::Color::Scrollbar);
    const float radius = (horizontalBar ? thumb.height : thumb.width) * 0.5F;
    ImGui::GetWindowDrawList()->AddRectFilled(ImGuiConverter::toImVec2(thumb.getMin()), ImGuiConverter::toImVec2(thumb.getMax()), ImGuiConverter::toImU32(color.withAlpha(color.a * alpha)), radius);
}

} // namespace haylen::ui
