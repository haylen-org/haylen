#include "haylen/input/AppTextField.hpp"

#include <utility>

namespace haylen::input {

void AppTextField::edit(State state) {
    const bool changed = state.text != field.text || state.selectionStart != field.selectionStart || state.selectionEnd != field.selectionEnd;
    if (changed) {
        ++field.revision;
    }
    const bool moved = state.bounds != field.bounds || state.options != field.options;
    field.text = std::move(state.text);
    field.selectionStart = state.selectionStart;
    field.selectionEnd = state.selectionEnd;
    field.bounds = state.bounds;
    field.caret = {state.bounds.getRight(), state.bounds.y, 0.0F, state.bounds.height};
    field.options = state.options;
    if (!editing || changed || moved) {
        textInput.edit(field);
    }
    editing = true;
}

void AppTextField::finish() {
    if (std::exchange(editing, false)) {
        textInput.finish();
    }
}

void AppTextField::receive(const platform::TextInput::Edit& edit) {
    if (edit.field != kField) {
        return;
    }
    field.text = edit.text;
    field.selectionStart = edit.selectionStart;
    field.selectionEnd = edit.selectionEnd;
}

} // namespace haylen::input
