#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/ReadbackBuffer.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// A copy of a render target into a pixel buffer object behind a fence, which `glReadPixels` queues without waiting. Float targets come back as floats and leave as bytes.
class PixelReadback final : public Device::Readback {
  public:
    PixelReadback(const std::shared_ptr<Context>& owner, Gl::Uint framebuffer, int width, int height, bool floats);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    static constexpr std::string_view kSubject = "the pixels of a render target";

    [[nodiscard]] static std::size_t getBytes(int width, int height, bool floats) noexcept;

    ReadbackBuffer copy;
    std::size_t pixels = 0;
    bool floating = false;
};

} // namespace haylen::gpu::opengl
