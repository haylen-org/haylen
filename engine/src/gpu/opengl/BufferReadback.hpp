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

// A copy of a range of a buffer into a buffer that the CPU reads behind a fence, which `glCopyBufferSubData` queues without waiting, and whose bytes come as they are.
class BufferReadback final : public Device::Readback {
  public:
    BufferReadback(const std::shared_ptr<Context>& owner, Gl::Uint source, std::size_t offset, std::size_t size);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    static constexpr std::string_view kSubject = "the bytes of a buffer";

    ReadbackBuffer copy;
};

} // namespace haylen::gpu::opengl
