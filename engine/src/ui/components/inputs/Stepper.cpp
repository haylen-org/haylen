#include "ui/components/inputs/Stepper.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/Surfaces.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void Stepper::readProperties(PropertyReader& reader) {
    double lowest = minimum;
    double highest = maximum;
    reader.read("value", value);
    reader.read("min", lowest);
    reader.read("max", highest);
    reader.read("step", increment);
    reader.read("decimals", decimals, 0, 6);
    reader.read("wrap", wrap);
    ChoiceItem::readList(reader, "items", items);
    reader.read("selected", selected);
    if (lowest > highest) {
        reader.fail("min", "must not be greater than max");
    }
    if (!(increment > 0.0)) {
        reader.fail("step", "must be greater than zero");
    }
    minimum = lowest;
    maximum = highest;
    value = std::clamp(value, minimum, maximum);

    // An option stepper always shows an option, the first one that can be picked unless another is selected.
    if (!items.empty() && ChoiceItem::indexOf(items, selected) < 0) {
        const auto enabled = std::ranges::find_if(items, [](const ChoiceItem& item) { return item.enabled; });
        selected = enabled != items.end() ? enabled->id : items.front().id;
    }
}

math::Vec2 Stepper::measureContent(Context& context, float availableWidth) {
    float widest = 0.0F;
    if (items.empty()) {
        widest = std::max(Typography::measure(context, Theme::Font::Body, Typography::formatNumber(minimum, decimals)).x, Typography::measure(context, Theme::Font::Body, Typography::formatNumber(maximum, decimals)).x);
    }
    for (const ChoiceItem& item : items) {
        widest = std::max(widest, Typography::measure(context, Theme::Font::Body, context.getText(item.text)).x);
    }
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    return {std::min(availableWidth, widest + height * 2.0F + context.getMetric(Theme::Metric::ControlPaddingX) * 2.0F), height};
}

void Stepper::render(Context& context, const math::Rect& bounds) {
    const float radius = context.getMetric(Theme::Metric::ControlRadius);
    const float side = bounds.height;
    const math::Rect previous{bounds.x, bounds.y, side, side};
    const math::Rect next{bounds.getRight() - side, bounds.y, side, side};
    Surfaces::draw(context, Theme::Surface::Field, bounds, context.getColor(Theme::Color::Raised), context.getColor(Theme::Color::Border));

    // The arrows come first, so they win the pointer over the value under them, even at the end of the range, and only the whole control takes the focus.
    int stepped = 0;
    for (const auto& [area, direction] : {std::pair{previous, -1}, std::pair{next, 1}}) {
        ImGui::PushID(direction);
        const bool possible = canStep(direction);
        const Widgets::Interaction arrow = Widgets::interact(context, area, radius, "##arrow", ImGuiButtonFlags_NoNavFocus);
        if (arrow.hovered && possible) {
            ImGui::GetWindowDrawList()->AddRectFilled(ImGuiConverter::toImVec2(area.getMin()), ImGuiConverter::toImVec2(area.getMax()), ImGuiConverter::toImU32(context.getColor(arrow.held ? Theme::Color::Pressed : Theme::Color::Hover)), radius);
        }
        Widgets::arrow(area.getCenter(), side * 0.28F, direction < 0 ? ImGuiDir_Left : ImGuiDir_Right, context.getColor(possible ? Theme::Color::TextMuted : Theme::Color::TextDisabled));
        ImGui::PopID();
        if (arrow.clicked) {
            stepped = direction;
        }
    }

    const Widgets::Interaction body = Widgets::interact(context, bounds, radius, "##value");
    if (takeFocusRequest()) {
        Widgets::focusItem(context);
    }
    Typography::drawAligned(context, Theme::Font::Body, math::Rect::fromMinMax({previous.getRight(), bounds.y}, {next.x, bounds.getBottom()}), context.getColor(Theme::Color::Text), getShownText(context), Alignment::Center);

    if (const std::optional<FocusDirection> direction = takeFocusDirection(context)) {
        stepped = direction == FocusDirection::Left ? -1 : 1;
    }
    if (stepped != 0 && canStep(stepped)) {
        step(context, stepped);
    } else if (body.clicked) {
        // Accept and a tap on the value move on to the next value and start over after the last one.
        const bool wrapped = std::exchange(wrap, true);
        step(context, 1);
        wrap = wrapped;
    }
}

std::string Stepper::getShownText(Context& context) const {
    if (items.empty()) {
        return Typography::formatNumber(value, decimals);
    }
    return context.getText(items[static_cast<std::size_t>(ChoiceItem::indexOf(items, selected))].text);
}

bool Stepper::canStep(int direction) const {
    if (wrap) {
        return items.empty() ? minimum < maximum : std::ranges::count_if(items, [](const ChoiceItem& item) { return item.enabled; }) > 1;
    }
    if (items.empty()) {
        return direction < 0 ? value > minimum : value < maximum;
    }
    const int index = ChoiceItem::indexOf(items, selected);
    for (int other = index + direction; other >= 0 && other < static_cast<int>(items.size()); other += direction) {
        if (items[static_cast<std::size_t>(other)].enabled) {
            return true;
        }
    }
    return false;
}

void Stepper::step(Context& context, int direction) {
    if (items.empty()) {
        double next = value + increment * direction;
        if (next > maximum + increment * 1e-9) {
            next = wrap && value >= maximum ? minimum : maximum;
        } else if (next < minimum - increment * 1e-9) {
            next = wrap && value <= minimum ? maximum : minimum;
        }
        next = std::clamp(next, minimum, maximum);
        if (next != value) {
            value = next;
            context.emit(*this, "change", {{"value", value}});
        }
        return;
    }

    const auto count = static_cast<int>(items.size());
    int index = ChoiceItem::indexOf(items, selected);
    for (int tried = 1; tried < count; ++tried) {
        const int other = index + direction * tried;
        if (!wrap && (other < 0 || other >= count)) {
            return;
        }
        const ChoiceItem& item = items[static_cast<std::size_t>((other % count + count) % count)];
        if (item.enabled) {
            selected = item.id;
            context.emit(*this, "change", {{"value", selected}});
            return;
        }
    }
}

} // namespace haylen::ui
