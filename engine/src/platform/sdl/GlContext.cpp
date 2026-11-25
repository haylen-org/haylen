#include "platform/sdl/GlContext.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "haylen/core/Log.hpp"

namespace haylen::platform::sdl {

GlContext::GlContext(SDL_Window* value) : window(value), context(SDL_GL_CreateContext(value)) {
    if (context == nullptr) {
        throw std::runtime_error("The window could not create its OpenGL context, since the driver of the GPU refuses the version or the pixel format that the app asks for. Update the driver, or let the app draw through another graphics API.");
    }
}

GlContext::GlContext(SDL_Window* value, const GlContext& shared) noexcept : window(value), context(shared.context), owned(false) {}

GlContext::~GlContext() {
    if (owned) {
        SDL_GL_DestroyContext(context);
    }
}

void GlContext::makeCurrent() {
    if (!SDL_GL_MakeCurrent(window, context)) {
        throw std::runtime_error("The window could not make its OpenGL context current.");
    }
}

void GlContext::swapBuffers() {
    if (!SDL_GL_SwapWindow(window)) {
        throw std::runtime_error("The window could not show the frame that its OpenGL context drew.");
    }
}

// A driver that refuses to turn vertical sync off keeps the frames at the pace of the display, which the log tells once.
void GlContext::setSwapInterval(int value) {
    if (SDL_GL_SetSwapInterval(value) || std::exchange(intervalRefused, true)) {
        return;
    }

    core::Log::warning("The OpenGL driver refuses the swap interval {}, so the window keeps the one it had.", value);
}

// The function wglGetProcAddress of Windows answers 1, 2, 3 or -1 instead of null for some functions, such as those of OpenGL 1.1, which opengl32.dll exports itself, and SDL hands those answers on.
void* GlContext::getProcAddress(const char* name) {
    void* address = reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
#if defined(_WIN32)
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (value >= -1 && value <= 3) {
        return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name));
    }
#endif
    return address;
}

} // namespace haylen::platform::sdl
