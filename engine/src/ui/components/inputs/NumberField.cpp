#include "ui/components/inputs/NumberField.hpp"

#include <algorithm>
#include <cmath>
#include <system_error>

#include <fast_float/fast_float.h>
#include <imgui.h>

#include "haylen/ui/Context.hpp"
#include "ui/TextEditor.hpp"
#include "ui/Typography.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

void NumberField::readProperties(PropertyReader& reader) {
    double lowest = minimum;
    double highest = maximum;
    reader.read("value", value);
    reader.read("min", lowest);
    reader.read("max", highest);
    reader.read("step", step, 0.0);
    reader.read("decimals", decimals, 0, 6);
    reader.readChoice<platform::TextInput::ReturnKey>("returnKey", returnKey, TextEditor::kReturnKeys);
    if (lowest > highest) {
        reader.fail("min", "must not be greater than \"max\"");
    }
    minimum = lowest;
    maximum = highest;
    value = std::clamp(value, minimum, maximum);
    text = Typography::formatNumber(value, decimals);
}

math::Vec2 NumberField::measureContent(Context& context, float availableWidth) {
    const float height = context.getMetric(Theme::Metric::ControlHeight);
    return {std::min(availableWidth, height * 2.0F + 160.0F), height};
}

void NumberField::render(Context& context, const math::Rect& bounds) {
    const float side = bounds.height;
    const math::Rect minus = context.mirror({bounds.x, bounds.y, side, side}, bounds);
    const math::Rect plus = context.mirror({bounds.getRight() - side, bounds.y, side, side}, bounds);
    const math::Rect middle = math::Rect::fromMinMax({bounds.x + side + 4.0F, bounds.y}, {bounds.getRight() - side - 4.0F, bounds.getBottom()});

    ImGui::PushID("minus");
    const bool lower = Widgets::button(context, minus, "-", nullptr, Widgets::ButtonVariant::Default);
    ImGui::PopID();
    ImGui::PushID("plus");
    const bool raise = Widgets::button(context, plus, "+", nullptr, Widgets::ButtonVariant::Default);
    ImGui::PopID();

    // Numeric keypads have no minus sign, so a field that takes negative numbers types on the text keyboard.
    const platform::TextInput::Keyboard keyboard = minimum < 0.0 ? platform::TextInput::Keyboard::Text : (decimals > 0 ? platform::TextInput::Keyboard::Decimal : platform::TextInput::Keyboard::Number);
    const TextEditor::Result typed = TextEditor::draw(context, middle, text, {.input = {.keyboard = keyboard, .returnKey = returnKey, .capitalization = platform::TextInput::Capitalization::None, .autocorrect = false}, .focus = takeFocusRequest()});
    const bool editing = ImGui::IsItemActive();
    const bool committed = typed.submitted || ImGui::IsItemDeactivated();
    double next = value;
    if (lower || raise) {
        next = std::clamp(value + (raise ? step : -step), minimum, maximum);
    } else if (typed.changed || committed) {
        // A typed number inside the range counts at once, and one outside it waits until the editing ends, which brings it into the range.
        double parsed = 0.0;
        const auto [end, error] = fast_float::from_chars(text.data(), text.data() + text.size(), parsed);
        const bool number = error == std::errc{} && end == text.data() + text.size() && std::isfinite(parsed);
        if (number && (committed || (parsed >= minimum && parsed <= maximum))) {
            next = std::clamp(parsed, minimum, maximum);
        }
    }
    if (!editing) {
        text = Typography::formatNumber(next, decimals);
    }
    if (next != value) {
        value = next;
        context.emit(*this, "change", {{"value", value}});
    }
}

} // namespace haylen::ui
