#include "ui/ScrollFade.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/math/Color.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

// Each fade runs from the color at the edge to nothing inside the box, as strong as the share of its length that the content continues past the edge.
void ScrollFade::draw(Context& context, const math::Rect& box, bool horizontal, double before, double after) {
    const float length = std::min(context.getMetric(Theme::Metric::ScrollFade), (horizontal ? box.width : box.height) * 0.5F);
    if (length <= 0.0F) {
        return;
    }
    const math::Color color = context.getColor(Theme::Color::ScrollFade);
    const ImU32 clear = ImGuiConverter::toImU32(color.withAlpha(0.0F));
    const ImU32 start = ImGuiConverter::toImU32(color.withAlpha(color.a * std::clamp(static_cast<float>(before) / length, 0.0F, 1.0F)));
    const ImU32 end = ImGuiConverter::toImU32(color.withAlpha(color.a * std::clamp(static_cast<float>(after) / length, 0.0F, 1.0F)));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(box.getMin()), ImGuiConverter::toImVec2(box.getMax()), true);
    if (horizontal) {
        list.AddRectFilledMultiColor({box.x, box.y}, {box.x + length, box.getBottom()}, start, clear, clear, start);
        list.AddRectFilledMultiColor({box.getRight() - length, box.y}, {box.getRight(), box.getBottom()}, clear, end, end, clear);
    } else {
        list.AddRectFilledMultiColor({box.x, box.y}, {box.getRight(), box.y + length}, start, start, clear, clear);
        list.AddRectFilledMultiColor({box.x, box.getBottom() - length}, {box.getRight(), box.getBottom()}, clear, clear, end, end);
    }
    list.PopClipRect();
}

} // namespace haylen::ui
