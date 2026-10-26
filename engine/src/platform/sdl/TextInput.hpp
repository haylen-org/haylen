#pragma once

#include <optional>

#include <SDL3/SDL_rect.h>

#include "haylen/platform/TextInput.hpp"

namespace haylen::platform::sdl {

class Window;

// Text input of Windows and Linux, which type through the text events of SDL. It tells SDL the line of the caret of the focused field, so the composition and candidate windows of the input methods of Windows, IBus and Fcitx open at the field, below the line being typed.
class TextInput final : public platform::TextInput {
  public:
    explicit TextInput(const Window& owner) noexcept : window(owner) {}

    void edit(const Field& field) override;
    void finish() override;

    // Tells SDL the line again once the window gains the focus, since input methods such as the XIM server of IBus forget the place of the caret of a window that lost it.
    void restore();

  private:
    void apply();

    const Window& window;

    // The line of the caret in the coordinates of the window and the place of the caret along it, or none while no field edits.
    std::optional<SDL_Rect> line;
    int cursor = 0;
};

} // namespace haylen::platform::sdl
