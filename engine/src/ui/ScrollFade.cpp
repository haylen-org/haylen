#include "ui/ScrollFade.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/math/Color.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

// Each fade runs from the color at the edge to nothing inside the box, as strong as the share of its length that the content continues past the edge. An edge where the content rests draws nothing.
void ScrollFade::draw(Context& context, const math::Rect& box, bool horizontal, double before, double after) {
    const float length = std::min(context.getMetric(Theme::Metric::ScrollFade), (horizontal ? box.width : box.height) * 0.5F);
    if (length <= 0.0F) {
        return;
    }

    const math::Color color = context.getColor(Theme::Color::ScrollFade);
    const float startStrength = std::clamp(static_cast<float>(before) / length, 0.0F, 1.0F);
    const float endStrength = std::clamp(static_cast<float>(after) / length, 0.0F, 1.0F);
    if (color.a <= 0.0F || (startStrength <= 0.0F && endStrength <= 0.0F)) {
        return;
    }

    const ImU32 clear = ImGuiConverter::toImU32(color.withAlpha(0.0F));
    const ImU32 start = ImGuiConverter::toImU32(color.withAlpha(color.a * startStrength));
    const ImU32 end = ImGuiConverter::toImU32(color.withAlpha(color.a * endStrength));
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(box.getMin()), ImGuiConverter::toImVec2(box.getMax()), true);
    if (startStrength > 0.0F) {
        const ImVec2 far = horizontal ? ImVec2{box.x + length, box.getBottom()} : ImVec2{box.getRight(), box.y + length};
        list.AddRectFilledMultiColor({box.x, box.y}, far, start, horizontal ? clear : start, clear, horizontal ? start : clear);
    }
    if (endStrength > 0.0F) {
        const ImVec2 near = horizontal ? ImVec2{box.getRight() - length, box.y} : ImVec2{box.x, box.getBottom() - length};
        list.AddRectFilledMultiColor(near, {box.getRight(), box.getBottom()}, clear, horizontal ? end : clear, end, horizontal ? clear : end);
    }
    list.PopClipRect();
}

} // namespace haylen::ui
