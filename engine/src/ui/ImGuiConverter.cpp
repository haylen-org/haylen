#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

ImVec2 ImGuiConverter::toImVec2(math::Vec2 value) noexcept {
    return {value.x, value.y};
}

ImVec4 ImGuiConverter::toImVec4(math::Color color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

ImRect ImGuiConverter::toImRect(const math::Rect& rect) noexcept {
    return {rect.x, rect.y, rect.getRight(), rect.getBottom()};
}

ImU32 ImGuiConverter::toImU32(math::Color color) noexcept {
    return ImGui::GetColorU32(toImVec4(color));
}

} // namespace haylen::ui
