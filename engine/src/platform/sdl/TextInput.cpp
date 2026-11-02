#include "platform/sdl/TextInput.hpp"

#include <cmath>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>

#include "platform/sdl/Window.hpp"

namespace haylen::platform::sdl {

// SDL places the input method by the area in the coordinates of the window, with the cursor as an offset from its left edge, while fields come in framebuffer pixels. The plain keyboard has no caret on screen and leaves the input method where the system puts it.
void TextInput::edit(const Field& field) {
    SDL_Window* handle = window.get();
    if (handle == nullptr) {
        return;
    }

    if (field.id == kKeyboardField) {
        line.reset();
        cursor = 0;
    } else {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(handle, &width, &height);
        const math::Vec2 pixels = window.getPixelSize();
        const float scale = pixels.x > 0.0F ? static_cast<float>(width) / pixels.x : 1.0F;
        const auto toWindow = [scale](float value) { return static_cast<int>(std::lround(value * scale)); };
        line = SDL_Rect{.x = toWindow(field.bounds.x), .y = toWindow(field.caret.y), .w = toWindow(field.bounds.width), .h = toWindow(field.caret.height)};
        cursor = toWindow(field.caret.x - field.bounds.x);
    }

    editing = true;
    apply();
    if (!SDL_TextInputActive(handle)) {
        SDL_StartTextInput(handle);
    }
}

void TextInput::finish() {
    line.reset();
    cursor = 0;
    editing = false;
    if (SDL_Window* handle = window.get(); handle != nullptr) {
        SDL_StopTextInput(handle);
    }
}

void TextInput::restore() {
    if (editing) {
        apply();
    }
}

void TextInput::apply() {
    if (SDL_Window* handle = window.get(); handle != nullptr) {
        SDL_SetTextInputArea(handle, line ? &*line : nullptr, cursor);
    }
}

} // namespace haylen::platform::sdl
