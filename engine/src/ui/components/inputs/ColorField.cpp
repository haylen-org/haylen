#include "ui/components/inputs/ColorField.hpp"

#include <algorithm>
#include <array>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void ColorField::readProperties(PropertyReader& reader) {
    reader.read("value", value);
    reader.read("alpha", alpha);
}

math::Vec2 ColorField::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 280.0F), context.getMetric(Theme::Metric::ControlHeight)};
}

void ColorField::render(Context& context, const math::Rect& bounds) {
    const Widgets::Interaction state = Widgets::interact(context, bounds, context.getMetric(Theme::Metric::ControlRadius), "##color");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    Surfaces::draw(context, Theme::Surface::Field, bounds, context.getColor(state.hovered ? Theme::Color::BorderStrong : Theme::Color::Raised), context.getColor(Theme::Color::Border));

    const float padding = context.getMetric(Theme::Metric::ControlPaddingY);
    const math::Rect swatch = context.mirror({bounds.x + padding, bounds.y + padding, bounds.height - padding * 2.0F, bounds.height - padding * 2.0F}, bounds);
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.AddRectFilled(ImGuiConverter::toImVec2(swatch.getMin()), ImGuiConverter::toImVec2(swatch.getMax()), ImGuiConverter::toImU32(value), context.getMetric(Theme::Metric::ControlRadius) * 0.5F);
    list.AddRect(ImGuiConverter::toImVec2(swatch.getMin()), ImGuiConverter::toImVec2(swatch.getMax()), ImGuiConverter::toImU32(context.getColor(Theme::Color::BorderStrong)), context.getMetric(Theme::Metric::ControlRadius) * 0.5F);
    Typography::drawAligned(context, Theme::Font::Monospace, context.mirror(math::Rect::fromMinMax({bounds.x + bounds.height, bounds.y}, {bounds.getRight() - padding, bounds.getBottom()}), bounds), context.getColor(Theme::Color::Text), value.toHex(), Alignment::Start);

    if (state.clicked) {
        ImGui::OpenPopup("##picker");
    }
    Widgets::placePopup(context, bounds, 360.0F);
    if (ImGui::BeginPopup("##picker")) {
        std::array<float, 4> channels{value.r, value.g, value.b, value.a};
        const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | (alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha);
        ImGui::SetNextItemWidth(360.0F);
        if (ImGui::ColorPicker4("##pick", channels.data(), flags)) {
            value = {channels[0], channels[1], channels[2], alpha ? channels[3] : 1.0F};
            context.emit(*this, "change", {{"value", value.toHex()}});
        }
        ImGui::EndPopup();
    }
}

} // namespace haylen::ui
