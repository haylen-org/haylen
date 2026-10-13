#include "ui/components/inputs/PlayArea.hpp"

#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

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
    Widgets::drawFocusRing(context, bounds, itemId, 0.0F);
}

bool PlayArea::describe(Context&, platform::AccessibilityElement& element) const {
    element.role = platform::AccessibilityElement::Role::PlayArea;
    element.actions = platform::AccessibilityElement::Activate;
    return true;
}

} // namespace haylen::ui
