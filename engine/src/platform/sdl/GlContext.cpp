#include "platform/sdl/GlContext.hpp"

#include <stdexcept>
#include <string>

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

void* GlContext::getProcAddress(const char* name) {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

} // namespace haylen::platform::sdl
