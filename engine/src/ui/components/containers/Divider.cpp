#include "ui/components/containers/Divider.hpp"

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Divider::readProperties(PropertyReader& reader) {
    reader.read("vertical", vertical);
    reader.read("color", color);
}

math::Vec2 Divider::measureContent(Context& context, float) {
    const float thickness = context.getMetric(Theme::Metric::BorderWidth);
    return {thickness, thickness};
}

void Divider::render(Context& context, const math::Rect& bounds) {
    const math::Vec2 center = bounds.getCenter();
    const ImVec2 from = vertical ? ImVec2{center.x, bounds.y} : ImVec2{bounds.x, center.y};
    const ImVec2 to = vertical ? ImVec2{center.x, bounds.getBottom()} : ImVec2{bounds.getRight(), center.y};
    ImGui::GetWindowDrawList()->AddLine(from, to, ImGuiConverter::toImU32(context.getColor(color.value_or(Theme::Color::Border))), context.getMetric(Theme::Metric::BorderWidth));
}

} // namespace haylen::ui
