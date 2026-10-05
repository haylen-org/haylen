#include "ui/components/containers/Accordion.hpp"

#include <algorithm>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"
#include "ui/components/collections/ListRow.hpp"

namespace haylen::ui {

void Accordion::readProperties(PropertyReader& reader) {
    ChoiceItem::readList(reader, "items", items);
    reader.read("multiple", multiple);
    if (const core::Json* listed = reader.take("expanded")) {
        if (!PropertyReader::isList(*listed) || !std::ranges::all_of(*listed, [](const core::Json& entry) { return entry.is_string(); })) {
            reader.fail("expanded", "must be a list of item ids");
        }
        expanded.clear();
        for (const core::Json& entry : *listed) {
            expanded.insert(entry.get<std::string>());
        }
    }
}

// An accordion fills the width it gets, and an unbounded width, such as the one of a horizontal scroll, gets the width of its widest header or open section.
math::Vec2 Accordion::measureContent(Context& context, float availableWidth) {
    const float header = context.getMetric(Theme::Metric::ControlHeight);
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float arrow = context.getMetric(Theme::Metric::IconSize);
    math::Vec2 size{0.0F, header * static_cast<float>(items.size())};
    for (std::size_t index = 0; index < items.size(); ++index) {
        size.x = std::max(size.x, Typography::measure(context, Theme::Font::Button, context.getText(items[index].text)).x + arrow + context.getMetric(Theme::Metric::RowPadding) * 3.0F);
        Component* section = getSection(index);
        if (section != nullptr && expanded.contains(items[index].id)) {
            const math::Vec2 measured = section->measure(context, availableWidth);
            size = {std::max(size.x, measured.x), size.y + measured.y + spacing * 2.0F};
        }
    }
    return {availableWidth < CommonProperties::kUnbounded ? availableWidth : size.x, size.y};
}

void Accordion::render(Context& context, const math::Rect& bounds) {
    const float header = context.getMetric(Theme::Metric::ControlHeight);
    const float spacing = context.getMetric(Theme::Metric::ItemSpacing);
    const float arrow = context.getMetric(Theme::Metric::IconSize);
    const std::string focused = takeFocusRequest() && !items.empty() ? items.front().id : std::string();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    float y = bounds.y;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const ChoiceItem& item = items[index];
        const bool open = expanded.contains(item.id);
        const math::Rect row{bounds.x, y, bounds.width, header};
        y += header;

        ImGui::PushID(item.id.c_str());
        ImGui::BeginDisabled(!item.enabled);
        const Widgets::Interaction state = ListRow::draw(context, row, false);
        if (item.id == focused) {
            Widgets::focusItem(context);
        }
        Widgets::arrow(context.mirror({row.x + context.getMetric(Theme::Metric::RowPadding), row.y, arrow, row.height}, row).getCenter(), arrow * 0.4F, Widgets::mirror(context, open ? ImGuiDir_Down : ImGuiDir_Right), context.getColor(Theme::Color::TextMuted));
        Typography::drawAligned(context, Theme::Font::Button, context.mirror(math::Rect::fromMinMax({row.x + context.getMetric(Theme::Metric::RowPadding) * 2.0F + arrow, row.y}, row.getMax()), row), context.getColor(Theme::Color::Text), context.getText(item.text), Alignment::Start);
        ImGui::EndDisabled();
        ImGui::PopID();
        list.AddLine({row.x, row.getBottom()}, {row.getRight(), row.getBottom()}, ImGuiConverter::toImU32(context.getColor(Theme::Color::Border)), context.getMetric(Theme::Metric::BorderWidth));
        if (state.clicked) {
            toggle(context, item);
        }

        Component* section = getSection(index);
        if (section == nullptr || !open || !expanded.contains(item.id)) {
            continue;
        }
        const float height = section->measure(context, bounds.width).y;
        section->draw(context, {bounds.x, y + spacing, bounds.width, height});
        y += height + spacing * 2.0F;
    }
}

Component* Accordion::getSection(std::size_t index) const {
    return index < getChildren().size() && getChildren()[index]->getCommon().visible ? getChildren()[index].get() : nullptr;
}

// A section that opens alone closes the others, and each of them reports its toggle first.
void Accordion::toggle(Context& context, const ChoiceItem& item) {
    const bool open = !expanded.contains(item.id);
    if (!open) {
        expanded.erase(item.id);
    } else {
        if (!multiple) {
            for (const ChoiceItem& other : items) {
                if (expanded.contains(other.id)) {
                    context.emit(*this, "toggle", {{"item", other.id}, {"expanded", false}});
                }
            }
            expanded.clear();
        }
        expanded.insert(item.id);
    }
    context.emit(*this, "toggle", {{"item", item.id}, {"expanded", open}});
}

} // namespace haylen::ui
