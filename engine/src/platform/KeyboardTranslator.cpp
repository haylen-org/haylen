#include "platform/KeyboardTranslator.hpp"

#include <algorithm>
#include <utility>

#include "haylen/core/Utf8.hpp"

namespace haylen::platform {

void KeyboardTranslator::reset() noexcept {
    committed.clear();
}

std::vector<Event> KeyboardTranslator::translate(const Event& event) {
    std::vector<Event> events;
    if (event.type == Event::Type::TextAction) {
        switch (event.textAction) {
        case TextInput::Action::Submit:
            press(events, input::Key::Enter);
            break;
        case TextInput::Action::Next:
            press(events, input::Key::Tab);
            break;
        case TextInput::Action::Cancel:
            press(events, input::Key::Escape);
            break;
        case TextInput::Action::Dismissed:
            break;
        }
        return events;
    }

    std::u32string text = core::Utf8::decode(event.textEdit.text);
    if (event.textEdit.isComposing()) {
        const auto start = std::min(static_cast<std::size_t>(event.textEdit.compositionStart), text.size());
        const auto end = std::min(static_cast<std::size_t>(event.textEdit.compositionEnd), text.size());
        text.erase(start, end - start);
    }

    // What differs from the text typed so far is erased with backspaces and typed again, the way autocorrection replaces a word.
    const auto kept = static_cast<std::size_t>(std::ranges::mismatch(committed, text).in1 - committed.begin());
    for (std::size_t index = kept; index < committed.size(); ++index) {
        press(events, input::Key::Backspace);
    }
    for (const char32_t character : std::u32string_view(text).substr(kept)) {
        if (character == U'\n') {
            press(events, input::Key::Enter);
        } else if (character == U'\t') {
            press(events, input::Key::Tab);
        } else {
            events.push_back({.type = Event::Type::Character, .character = character});
        }
    }
    committed = std::move(text);
    return events;
}

void KeyboardTranslator::press(std::vector<Event>& events, input::Key key) {
    events.push_back({.type = Event::Type::KeyDown, .key = key});
    events.push_back({.type = Event::Type::KeyUp, .key = key});
}

} // namespace haylen::platform
