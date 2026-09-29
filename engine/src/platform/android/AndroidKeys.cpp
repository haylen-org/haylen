#include "platform/android/AndroidKeys.hpp"

#include <android/input.h>

#include "haylen/platform/Event.hpp"
#include "platform/android/AndroidTextInput.hpp"
#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

bool AndroidKeys::backTaken = false;
bool AndroidKeys::backDismissed = false;

bool AndroidKeys::handleEvent(const void* source) {
    const auto* event = static_cast<const AInputEvent*>(source);
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_KEY) {
        return false;
    }
    const std::int32_t code = AKeyEvent_getKeyCode(event);
    const std::optional<input::Key> key = toKey(code);
    if (!key) {
        return false;
    }

    // While a text field of the UI edits, its hidden field moves the caret and turns return and escape into actions.
    if (code != AKEYCODE_BACK && AndroidTextInput::isEditing()) {
        return false;
    }
    const std::int32_t action = AKeyEvent_getAction(event);

    // Back while a text field edits lets the field go and never leaves the app. An open keyboard takes the press itself to close, so only its release comes here.
    if (code == AKEYCODE_BACK && action == AKEY_EVENT_ACTION_DOWN && AKeyEvent_getRepeatCount(event) == 0) {
        backDismissed = AndroidTextInput::dismiss();
    }
    if (code == AKEYCODE_BACK && (backDismissed || (action == AKEY_EVENT_ACTION_UP && AndroidTextInput::isEditing()))) {
        return true;
    }

    // The press of back decides for its release too, so Android never sees half of it.
    if (code == AKEYCODE_BACK) {
        if (action == AKEY_EVENT_ACTION_DOWN && AKeyEvent_getRepeatCount(event) == 0) {
            backTaken = SokolRuntime::isBackCaptured();
        }
        if (!backTaken) {
            return false;
        }
    }
    if (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_UP) {
        SokolRuntime::handleEvent({.type = action == AKEY_EVENT_ACTION_DOWN ? Event::Type::KeyDown : Event::Type::KeyUp, .key = *key, .repeat = AKeyEvent_getRepeatCount(event) > 0});
    }
    return true;
}

std::optional<input::Key> AndroidKeys::toKey(std::int32_t code) noexcept {
    switch (code) {
    case AKEYCODE_DPAD_UP:
        return input::Key::Up;
    case AKEYCODE_DPAD_DOWN:
        return input::Key::Down;
    case AKEYCODE_DPAD_LEFT:
        return input::Key::Left;
    case AKEYCODE_DPAD_RIGHT:
        return input::Key::Right;
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_ENTER:
        return input::Key::Enter;
    case AKEYCODE_NUMPAD_ENTER:
        return input::Key::KeypadEnter;
    case AKEYCODE_BACK:
    case AKEYCODE_ESCAPE:
        return input::Key::Escape;
    case AKEYCODE_MEDIA_PLAY_PAUSE:
        return input::Key::Pause;
    default:
        return std::nullopt;
    }
}

} // namespace haylen::platform
