#include "platform/web/WebTextInput.hpp"

#include <emscripten/emscripten.h>

#include <string>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"

// clang-format off
EM_JS(void, haylen_js_text_edit, (const char* json), {
    Module.haylen.textInput.edit(JSON.parse(UTF8ToString(json)));
});

EM_JS(void, haylen_js_text_finish, (), {
    Module.haylen.textInput.finish();
});

EM_JS(void, haylen_js_text_fields, (const char* json), {
    Module.haylen.textInput.setFields(JSON.parse(UTF8ToString(json)));
});
// clang-format on

namespace haylen::platform {

void WebTextInput::edit(const Field& field) {
    haylen_js_text_edit(toJson(field).dump(-1, ' ', false, core::Json::error_handler_t::replace).c_str());
}

void WebTextInput::finish() {
    haylen_js_text_finish();
}

void WebTextInput::setVisibleFields(std::span<const Field> fields) {
    core::Json list = core::Json::array();
    for (const Field& field : fields) {
        list.push_back(toJson(field));
    }
    haylen_js_text_fields(list.dump().c_str());
}

// The page reports edits and actions from its own event handlers, which may run inside a call of the engine, such as the blur of finish, so they wait for the next frame.
void WebTextInput::receiveEdit(double field, double revision, const char* text, int selectionStart, int selectionEnd, int compositionStart, int compositionEnd) {
    SokolRuntime::postEvent({.type = Event::Type::TextEdited, .textEdit = {.field = static_cast<std::uint64_t>(field), .revision = static_cast<std::uint64_t>(revision), .text = text, .selectionStart = selectionStart, .selectionEnd = selectionEnd, .compositionStart = compositionStart, .compositionEnd = compositionEnd}});
}

void WebTextInput::receiveAction(double field, int action) {
    SokolRuntime::postEvent({.type = Event::Type::TextAction, .textEdit = {.field = static_cast<std::uint64_t>(field)}, .textAction = static_cast<Action>(action)});
}

void WebTextInput::receiveKeyboard(float x, float y, float width, float height) {
    SokolRuntime::postEvent({.type = Event::Type::KeyboardChanged, .keyboardFrame = {x, y, width, height}});
}

core::Json WebTextInput::toJson(const Field& field) {
    const Options& options = field.options;
    return {
        {"id", field.id}, {"revision", field.revision}, {"text", field.text}, {"selectionStart", field.selectionStart}, {"selectionEnd", field.selectionEnd}, {"bounds", {core::JsonNumber::fromFloat(field.bounds.x), core::JsonNumber::fromFloat(field.bounds.y), core::JsonNumber::fromFloat(field.bounds.width), core::JsonNumber::fromFloat(field.bounds.height)}}, {"multiline", options.keyboard == Keyboard::Multiline}, {"password", options.keyboard == Keyboard::Password}, {"inputMode", getInputMode(options.keyboard)}, {"enterKeyHint", getEnterKeyHint(options.returnKey)}, {"autocapitalize", getCapitalization(options.capitalization)}, {"autocorrect", options.autocorrect}, {"maxLength", options.maxLength},
    };
}

std::string_view WebTextInput::getInputMode(Keyboard keyboard) noexcept {
    switch (keyboard) {
    case Keyboard::Number:
        return "numeric";
    case Keyboard::Decimal:
        return "decimal";
    case Keyboard::Phone:
        return "tel";
    case Keyboard::Email:
        return "email";
    case Keyboard::Url:
        return "url";
    case Keyboard::Search:
        return "search";
    default:
        return "text";
    }
}

// An empty hint leaves the label of the return key to the browser.
std::string_view WebTextInput::getEnterKeyHint(ReturnKey key) noexcept {
    switch (key) {
    case ReturnKey::Done:
        return "done";
    case ReturnKey::Go:
        return "go";
    case ReturnKey::Next:
        return "next";
    case ReturnKey::Search:
        return "search";
    case ReturnKey::Send:
        return "send";
    case ReturnKey::Default:
        return "";
    }
    return "";
}

std::string_view WebTextInput::getCapitalization(Capitalization capitalization) noexcept {
    switch (capitalization) {
    case Capitalization::Sentences:
        return "sentences";
    case Capitalization::Words:
        return "words";
    case Capitalization::Characters:
        return "characters";
    case Capitalization::None:
        return "off";
    }
    return "off";
}

} // namespace haylen::platform
