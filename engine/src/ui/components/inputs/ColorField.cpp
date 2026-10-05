#include "ui/components/inputs/ColorField.hpp"

#include <algorithm>
#include <array>
#include <optional>

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
    const float radius = context.getMetric(Theme::Metric::ControlRadius) * 0.5F;
    Surfaces::drawShape(context, {.bounds = swatch, .radii = {radius, radius, radius, radius}, .color = value, .borderWidth = 1.0F, .borderColor = context.getColor(Theme::Color::BorderStrong)});
    Typography::drawAligned(context, Theme::Font::Monospace, context.mirror(math::Rect::fromMinMax({bounds.x + bounds.height, bounds.y}, {bounds.getRight() - padding, bounds.getBottom()}), bounds), context.getColor(Theme::Color::Text), value.toHex(), Alignment::Start);

    if (state.clicked) {
        ImGui::OpenPopup("##picker");
    }
    if (!ImGui::IsPopupOpen("##picker")) {
        return;
    }
    if (const std::optional<math::Rect> box = popup.begin(context, "##picker", ImGuiWindowFlags_None, context.getMetric(Theme::Metric::PanelPadding), {kPickerWidth, pickerHeight}, bounds, kPickerWidth)) {
        std::array<float, 4> channels{value.r, value.g, value.b, value.a};
        const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | (alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha);
        ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(box->getMin()));
        ImGui::SetNextItemWidth(kPickerWidth);
        if (ImGui::ColorPicker4("##pick", channels.data(), flags)) {
            value = {channels[0], channels[1], channels[2], alpha ? channels[3] : 1.0F};
            context.emit(*this, "change", {{"value", value.toHex()}});
        }
        pickerHeight = ImGui::GetItemRectSize().y;
        popup.end(context);
    }
}

} // namespace haylen::ui
