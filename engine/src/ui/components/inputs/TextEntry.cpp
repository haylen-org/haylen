#include "ui/components/inputs/TextEntry.hpp"

#include <algorithm>
#include <string>

#include "haylen/ui/Context.hpp"
#include "ui/TextEditor.hpp"

namespace haylen::ui {

void TextEntry::readProperties(PropertyReader& reader) {
    reader.read("value", value);
    reader.read("placeholder", placeholder);
    reader.read("maxLength", maxLength, 0, 1 << 20);
    reader.readChoice<platform::TextInput::ReturnKey>("returnKey", returnKey, platform::TextInput::kReturnKeys);
    if (reader.has("autocorrect")) {
        bool enabled = true;
        reader.read("autocorrect", enabled);
        autocorrect = enabled;
    }
    if (reader.has("autocapitalize")) {
        platform::TextInput::Capitalization mode = platform::TextInput::Capitalization::None;
        reader.readChoice<platform::TextInput::Capitalization>("autocapitalize", mode, platform::TextInput::kCapitalizations);
        capitalization = mode;
    }
    readMore(reader);
}

math::Vec2 TextEntry::measureContent(Context& context, float availableWidth) {
    return {std::min(availableWidth, 320.0F), context.getMetric(Theme::Metric::ControlHeight)};
}

platform::TextInput::Options TextEntry::getInputOptions(platform::TextInput::Keyboard keyboard) const noexcept {
    const bool prose = keyboard == platform::TextInput::Keyboard::Text || keyboard == platform::TextInput::Keyboard::Multiline;
    const bool corrected = prose || keyboard == platform::TextInput::Keyboard::Search;
    return {.keyboard = keyboard, .returnKey = returnKey, .capitalization = capitalization.value_or(prose ? platform::TextInput::Capitalization::Sentences : platform::TextInput::Capitalization::None), .autocorrect = autocorrect.value_or(corrected), .maxLength = maxLength};
}

void TextEntry::drawEntry(Context& context, const math::Rect& bounds, platform::TextInput::Keyboard keyboard, float reserveStart, float reserveEnd) {
    const std::string hint = context.getText(placeholder);
    const TextEditor::Result result = TextEditor::draw(context, bounds, value, {.placeholder = hint, .input = getInputOptions(keyboard), .focus = takeFocusRequest(), .reserveStart = reserveStart, .reserveEnd = reserveEnd});
    if (result.changed) {
        context.emit(*this, "change", {{"value", value}});
    }
    if (result.submitted) {
        context.emit(*this, "submit", {{"value", value}});
    }
}

} // namespace haylen::ui
