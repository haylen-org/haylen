#include "ui/components/inputs/PlayArea.hpp"

#include <algorithm>

#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void PlayArea::readProperties(PropertyReader& reader) {
    reader.read("radius", radius, 0.0F, 10000.0F);
    if (reader.has("cornerColor")) {
        math::Color color;
        reader.read("cornerColor", color);
        cornerColor = color;
    }
}

// A press on the area takes the focus when no control of the last frame lay under the pointer, such as touch controls drawn over the game.
void PlayArea::render(Context& context, const math::Rect& bounds) {
    const ImGuiID itemId = ImGui::GetID("##play");
    ImGui::ItemAdd(ImGuiConverter::toImRect(bounds), itemId);
    context.getFocus().addPlayArea(itemId, bounds);

    const ImGuiContext& state = *GImGui;
    const bool pressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
    const bool here = pressed && state.HoveredIdPreviousFrame == 0 && state.OpenPopupStack.Size == 0 && ImGui::IsWindowHovered() && bounds.contains(math::Vec2{state.IO.MousePos.x, state.IO.MousePos.y});
    if (takeFocusRequest() || here) {
        Widgets::focusItem(context);
    }
    if (radius > 0.0F) {
        drawCorners(context, bounds);
    }
    Widgets::drawFocusRing(context, bounds, itemId, radius);
}

// A border as wide as the radius runs around the area with its inner edge on the rounded corners of the area, clipped to the area, so it covers the corners of the game and nothing past the edges.
void PlayArea::drawCorners(Context& context, const math::Rect& bounds) const {
    const float corner = std::min(radius, std::min(bounds.width, bounds.height) * 0.5F);
    const float reach = corner * 2.0F;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(ImGuiConverter::toImVec2(bounds.getMin()), ImGuiConverter::toImVec2(bounds.getMax()), true);
    Surfaces::drawShape(context, {.bounds = bounds.inset(math::Insets::uniform(-corner)), .radii = {reach, reach, reach, reach}, .color = math::Color::transparent(), .borderWidth = corner, .borderColor = cornerColor.value_or(context.getColor(Theme::Color::Window))});
    list.PopClipRect();
}

bool PlayArea::describe(Context&, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::PlayArea;
    element.actions = platform::AccessibilityElement::Activate;
    return true;
}

} // namespace haylen::ui
