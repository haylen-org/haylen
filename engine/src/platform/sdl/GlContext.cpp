#include "platform/sdl/GlContext.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace haylen::platform::sdl {

GlContext::GlContext(SDL_Window* value) : window(value), context(SDL_GL_CreateContext(value)) {
    if (context == nullptr) {
        throw std::runtime_error("The window could not create its OpenGL context. " + std::string(SDL_GetError()));
    }
}

GlContext::~GlContext() {
    SDL_GL_DestroyContext(context);
}

void GlContext::makeCurrent() {
    SDL_GL_MakeCurrent(window, context);
}

void GlContext::swapBuffers() {
    SDL_GL_SwapWindow(window);
}

void GlContext::setSwapInterval(int value) {
    SDL_GL_SetSwapInterval(value);
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
