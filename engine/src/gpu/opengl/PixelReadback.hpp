#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "gpu/opengl/Context.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// A copy of a render target into a pixel buffer object behind a fence, which `glReadPixels` queues without waiting. Polling reads the status of the fence without blocking, which WebGL2 allows on the main thread, and copies the pixels out once the GPU passed it. Float targets come back as floats and leave as bytes.
class PixelReadback final : public Device::Readback {
  public:
    PixelReadback(const std::shared_ptr<Context>& owner, Gl::Uint framebuffer, int width, int height, bool floats);
    PixelReadback(const PixelReadback&) = delete;
    PixelReadback& operator=(const PixelReadback&) = delete;
    ~PixelReadback() override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    [[nodiscard]] std::vector<std::uint8_t> copyOut(Context& owner) const;
    void release(Context& owner) noexcept;

    std::weak_ptr<Context> context;
    Gl::Uint buffer = 0;
    Gl::Sync fence = nullptr;
    std::size_t pixels = 0;
    bool floating = false;
};

} // namespace haylen::gpu::opengl
