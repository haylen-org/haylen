#pragma once

#include <cstdint>
#include <string>

#include "haylen/math/Rect.hpp"
#include "haylen/platform/TextInput.hpp"

namespace haylen::input {

// A text field that the app draws itself, such as one of a GUI written in Lua, which edits through the native text input of the platform, with its on-screen keyboard and its input methods. One field edits at a time, and a text field of the UI that takes the focus takes the keyboard over.
class AppTextField final {
  public:
    // The id of the field in text events, which no field of the UI takes.
    static constexpr std::uint64_t kField = 1ULL << 32U;

    // What the app shows: the text, the selection in code points and the place of the field in framebuffer pixels.
    struct State {
        std::string text;
        int selectionStart = 0;
        int selectionEnd = 0;
        math::Rect bounds;
        platform::TextInput::Options options;
    };

    explicit AppTextField(platform::TextInput& input) : textInput(input) {}

    // Starts editing or follows the field. The native field takes the text and the selection only when they differ from what the user typed last, so an app that shows the typed text back keeps a composition going.
    void edit(State state);
    void finish();
    [[nodiscard]] bool isEditing() const noexcept {
        return editing;
    }

    // Keeps what the user typed into the field, which the app receives as a `TextEdited` event.
    void receive(const platform::TextInput::Edit& edit);

  private:
    platform::TextInput& textInput;
    platform::TextInput::Field field{.id = kField};
    bool editing = false;
};

} // namespace haylen::input
