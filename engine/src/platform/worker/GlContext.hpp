#pragma once

#include <emscripten/html5_webgl.h>

#include "haylen/core/AppConfig.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::platform::worker {

// The WebGL2 context of the `OffscreenCanvas` that the page handed the worker, through which the WebGL2 device draws.
class GlContext final : public gpu::GlContext {
  public:
    // Creates the context with the pixel format that the options of the window ask for and makes it current. Throws `std::runtime_error` when the canvas gives no WebGL2 context.
    explicit GlContext(const core::AppConfig::Window& options);
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    ~GlContext() override;

    void makeCurrent() override;
    void swapBuffers() override;
    void setSwapInterval(int value) override;
    [[nodiscard]] void* getProcAddress(const char* name) override;

  private:
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
};

} // namespace haylen::platform::worker
