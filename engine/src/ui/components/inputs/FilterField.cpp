#include "ui/components/inputs/FilterField.hpp"

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void FilterField::render(Context& context, const math::Rect& bounds) {
    const float icon = bounds.height * 0.6F;
    drawEntry(context, bounds, platform::TextInput::Keyboard::Search, icon, value.empty() ? 0.0F : icon);

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImU32 muted = ImGuiConverter::toImU32(context.getColor(Theme::Color::TextMuted));
    const math::Vec2 lens{bounds.x + icon * 0.65F, bounds.getCenter().y - icon * 0.08F};
    const float radius = icon * 0.22F;
    list.AddCircle(ImGuiConverter::toImVec2(lens), radius, muted, 0, 2.0F);
    list.AddLine({lens.x + radius * 0.7F, lens.y + radius * 0.7F}, {lens.x + radius * 1.6F, lens.y + radius * 1.6F}, muted, 2.0F);

    if (value.empty()) {
        return;
    }
    const math::Rect clear{bounds.getRight() - icon, bounds.y, icon, bounds.height};
    const Widgets::Interaction state = Widgets::interact(context, clear, context.getMetric(Theme::Metric::ControlRadius), "##clear");
    const math::Vec2 center = clear.getCenter() - math::Vec2{icon * 0.2F, 0.0F};
    const float arm = icon * 0.15F;
    const ImU32 color = ImGuiConverter::toImU32(context.getColor(state.hovered ? Theme::Color::Text : Theme::Color::TextMuted));
    list.AddLine({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, color, 2.0F);
    list.AddLine({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, color, 2.0F);
    if (state.clicked) {
        value.clear();
        context.emit(*this, "change", {{"value", value}});
    }
}

} // namespace haylen::ui
