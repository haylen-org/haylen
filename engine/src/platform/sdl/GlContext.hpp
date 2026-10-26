#pragma once

#include <SDL3/SDL_video.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::platform::sdl {

// The context of OpenGL, OpenGL ES or WebGL2 that SDL creates for a window opened for one of those APIs, through which their devices draw into the window.
class GlContext final : public gpu::GlContext {
  public:
    // Creates the context of the window and makes it current. Throws `std::runtime_error` when the driver refuses the version or the pixel format the window asked for.
    explicit GlContext(SDL_Window* value);
    ~GlContext() override;

    void makeCurrent() override;
    void swapBuffers() override;
    void setSwapInterval(int value) override;
    [[nodiscard]] void* getProcAddress(const char* name) override;

  private:
    SDL_Window* window;
    SDL_GLContext context;
};

} // namespace haylen::platform::sdl
