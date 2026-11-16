#include "platform/SharedTextInput.hpp"

#include <utility>

namespace haylen::platform {

void SharedTextInput::edit(const Field& field) {
    held = true;
    if (field.id == kKeyboardField) {
        showKeyboard(field, false);
        return;
    }

    input.edit(field);
    keyboardShown = false;
    shown = true;
}

// The plain keyboard of the app takes the input back from the editor that finishes.
void SharedTextInput::finish() {
    held = false;
    if (keyboardWanted) {
        showKeyboard(getKeyboardField(), true);
        return;
    }

    release();
}

void SharedTextInput::setKeyboardVisible(bool value) {
    keyboardWanted = value;
    if (held) {
        return;
    }

    if (value) {
        showKeyboard(getKeyboardField(), true);
        return;
    }

    release();
}

void SharedTextInput::showKeyboard(const Field& field, bool again) {
    if (again || !keyboardShown) {
        ++keyboardRevision;
    }

    Field keyboard = field;
    keyboard.revision = keyboardRevision;
    input.edit(keyboard);
    keyboardShown = true;
    shown = true;
}

void SharedTextInput::release() {
    keyboardShown = false;
    if (std::exchange(shown, false)) {
        input.finish();
    }
}

} // namespace haylen::platform
