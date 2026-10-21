#include "platform/windows/WindowsTextInput.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <imm.h>

#include "platform/NativeWindow.hpp"

namespace haylen::platform {

// The framebuffer covers the client area, with fewer pixels when the app turns high DPI off, so the caret turns into pixels of the client area. The plain keyboard has no caret on screen and leaves the input method where the system puts it.
void WindowsTextInput::edit(const Field& field) {
    if (field.id == kKeyboardField) {
        return;
    }
    const auto window = static_cast<HWND>(NativeWindow::getHandle());
    const HIMC context = ImmGetContext(window);
    if (context == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(window, &client);
    const float scale = NativeWindow::getFramebufferSize().x > 0.0F ? static_cast<float>(client.right) / NativeWindow::getFramebufferSize().x : 1.0F;
    const auto toClient = [scale](float value) { return static_cast<LONG>(value * scale); };
    const POINT caret{.x = toClient(field.caret.x), .y = toClient(field.caret.y)};
    const RECT line{.left = caret.x, .top = caret.y, .right = toClient(field.caret.getRight()), .bottom = toClient(field.caret.getBottom())};
    COMPOSITIONFORM composition{.dwStyle = CFS_FORCE_POSITION, .ptCurrentPos = caret, .rcArea = line};
    ImmSetCompositionWindow(context, &composition);

    // The candidate list opens below the caret and never covers the line being typed.
    CANDIDATEFORM candidates{.dwIndex = 0, .dwStyle = CFS_EXCLUDE, .ptCurrentPos = {.x = caret.x, .y = line.bottom}, .rcArea = line};
    ImmSetCandidateWindow(context, &candidates);
    ImmReleaseContext(window, context);
}

} // namespace haylen::platform
