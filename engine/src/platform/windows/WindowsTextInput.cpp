#include "platform/windows/WindowsTextInput.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <imm.h>

#include "sokol_app.h"

namespace haylen::platform {

// The framebuffer covers the client area pixel for pixel, so the caret needs no conversion. The plain keyboard has no caret on screen and leaves the input method where the system puts it.
void WindowsTextInput::edit(const Field& field) {
    if (field.id == kKeyboardField) {
        return;
    }
    const auto window = static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()));
    const HIMC context = ImmGetContext(window);
    if (context == nullptr) {
        return;
    }

    const POINT caret{.x = static_cast<LONG>(field.caret.x), .y = static_cast<LONG>(field.caret.y)};
    const RECT line{.left = caret.x, .top = caret.y, .right = static_cast<LONG>(field.caret.getRight()), .bottom = static_cast<LONG>(field.caret.getBottom())};
    COMPOSITIONFORM composition{.dwStyle = CFS_FORCE_POSITION, .ptCurrentPos = caret, .rcArea = line};
    ImmSetCompositionWindow(context, &composition);

    // The candidate list opens below the caret and never covers the line being typed.
    CANDIDATEFORM candidates{.dwIndex = 0, .dwStyle = CFS_EXCLUDE, .ptCurrentPos = {.x = caret.x, .y = line.bottom}, .rcArea = line};
    ImmSetCandidateWindow(context, &candidates);
    ImmReleaseContext(window, context);
}

} // namespace haylen::platform
