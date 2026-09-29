#include "ui/components/buttons/Chip.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Chip::readProperties(PropertyReader& reader) {
    reader.read("text", text);
    reader.read("selected", selected);
    reader.read("removable", removable);
}

math::Vec2 Chip::measureContent(Context& context, float) {
    const float height = context.getMetric(Theme::Metric::ControlHeight) * 0.75F;
    const float padding = context.getMetric(Theme::Metric::ControlPaddingX) * 0.75F;
    const float remove = removable ? height * 0.6F : 0.0F;
    return {Typography::measure(context, Theme::Font::Caption, context.getText(text)).x + padding * 2.0F + remove, height};
}

void Chip::render(Context& context, const math::Rect& bounds) {
    const float remove = removable ? bounds.height * 0.6F : 0.0F;
    const math::Rect body{bounds.x, bounds.y, bounds.width - remove, bounds.height};
    const Widgets::Interaction state = Widgets::interact(context, body, body.height * 0.5F, "##chip");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }

    const Theme::Surface surface = selected ? Theme::Surface::ChipSelected : Theme::Surface::Chip;
    math::Color fill = context.getColor(selected ? Theme::Color::AccentBackground : Theme::Color::Raised);
    if (state.hovered) {
        fill = math::Color{fill.r, fill.g, fill.b, std::min(1.0F, fill.a + 0.1F)};
    }
    Surfaces::draw(context, surface, bounds, fill, context.getColor(selected ? Theme::Color::Accent : Theme::Color::Border), bounds.height * 0.5F);
    const math::Color ink = context.getColor(selected ? Theme::Color::AccentText : Theme::Color::Text);
    Typography::drawAligned(context, Theme::Font::Caption, body, ink, context.getText(text), Alignment::Center);

    if (removable) {
        const math::Rect cross{body.getRight(), bounds.y, remove, bounds.height};
        const Widgets::Interaction removal = Widgets::interact(context, cross, cross.height * 0.5F, "##remove");
        const math::Vec2 center = cross.getCenter() - math::Vec2{remove * 0.25F, 0.0F};
        const float arm = remove * 0.18F;
        ImDrawList& list = *ImGui::GetWindowDrawList();
        const ImU32 color = ImGuiConverter::toImU32(context.getColor(removal.hovered ? Theme::Color::DangerText : Theme::Color::TextMuted));
        list.AddLine({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, color, 2.0F);
        list.AddLine({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, color, 2.0F);
        if (removal.clicked) {
            context.emit(*this, "remove");
        }
    }

    if (state.clicked) {
        selected = !selected;
        context.emit(*this, "change", {{"selected", selected}});
    }
}

} // namespace haylen::ui
