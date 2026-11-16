#pragma once

#include <cstdint>
#include <span>

#include "haylen/platform/TextInput.hpp"

namespace haylen::platform {

// The one native text input of the platform, which the editors of the engine and the plain keyboard of the window share. An editor, a text field of the UI, the field of the app or the plain keyboard that the UI opens for its ImGui inputs, holds it from its edit until its finish. The plain keyboard that the app asks for with `Window::setKeyboardVisible` shows while no editor holds the input, so it opens once the editor finishes, and asking for it or closing it while an editor holds the input never takes the input from the editor. Every plain keyboard reaches the platform with a revision of its own, so the platform starts it empty.
class SharedTextInput final : public TextInput {
  public:
    explicit SharedTextInput(TextInput& platformInput) noexcept : input(platformInput) {}

    [[nodiscard]] bool isNative() const noexcept override {
        return input.isNative();
    }
    void edit(const Field& field) override;
    void finish() override;
    void setVisibleFields(std::span<const Field> fields, float tapSlop) override {
        input.setVisibleFields(fields, tapSlop);
    }

    // Opens or closes the plain keyboard of the app.
    void setKeyboardVisible(bool value);

  private:
    // Shows the plain keyboard, as a new one unless the platform shows it already and `again` is false.
    void showKeyboard(const Field& field, bool again);

    // Ends the editing of the platform when it shows anything.
    void release();

    TextInput& input;
    std::uint64_t keyboardRevision = 0;
    bool held = false;
    bool keyboardWanted = false;
    bool keyboardShown = false;
    bool shown = false;
};

} // namespace haylen::platform
