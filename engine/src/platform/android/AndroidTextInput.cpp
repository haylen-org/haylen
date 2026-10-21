#include "platform/android/AndroidTextInput.hpp"

#include <cmath>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#include "platform/android/JavaBridge.hpp"

namespace haylen::platform {

bool AndroidTextInput::editing = false;

// The window counts pixels of the screen, while the framebuffer may be smaller when the app turns high DPI off. The options travel as the numbers of their enums.
void AndroidTextInput::edit(const Field& value) {
    const float scale = NativeWindow::getScale();
    const Options& options = value.options;
    const core::Json json = {
        {"id", value.id}, {"revision", value.revision}, {"text", value.text}, {"selectionStart", value.selectionStart}, {"selectionEnd", value.selectionEnd}, {"x", std::lround(value.bounds.x / scale)}, {"y", std::lround(value.bounds.y / scale)}, {"width", std::lround(value.bounds.width / scale)}, {"height", std::lround(value.bounds.height / scale)}, {"keyboard", static_cast<int>(options.keyboard)}, {"returnKey", static_cast<int>(options.returnKey)}, {"capitalization", static_cast<int>(options.capitalization)}, {"autocorrect", options.autocorrect}, {"maxLength", options.maxLength},
    };
    JavaBridge::editText(json.dump(-1, ' ', false, core::Json::error_handler_t::replace));
    editing = value.id != kKeyboardField;
}

void AndroidTextInput::finish() {
    JavaBridge::finishText();
    editing = false;
}

void AndroidTextInput::receiveEdit(JNIEnv& env, jlong field, jlong revision, jbyteArray text, jint selectionStart, jint selectionEnd, jint compositionStart, jint compositionEnd) {
    Player::postEvent({.type = Event::Type::TextEdited, .textEdit = {.field = static_cast<std::uint64_t>(field), .revision = static_cast<std::uint64_t>(revision), .text = JavaBridge::toString(env, text), .selectionStart = selectionStart, .selectionEnd = selectionEnd, .compositionStart = compositionStart, .compositionEnd = compositionEnd}});
}

void AndroidTextInput::receiveAction(jlong field, jint action) {
    Player::postEvent({.type = Event::Type::TextAction, .textEdit = {.field = static_cast<std::uint64_t>(field)}, .textAction = static_cast<Action>(action)});
}

// The frame arrives in window pixels, which the runtime turns into framebuffer pixels on the frame thread.
void AndroidTextInput::receiveKeyboard(jint x, jint y, jint width, jint height) {
    Player::postEvent({.type = Event::Type::KeyboardChanged, .keyboardFrame = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)}});
}

} // namespace haylen::platform
