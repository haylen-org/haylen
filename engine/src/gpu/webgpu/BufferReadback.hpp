#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/webgpu/ReadbackBuffer.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::webgpu {

// A copy of a range of a buffer into a buffer that the CPU maps once the GPU finished it, without waiting for it, which hands the bytes over as they are.
class BufferReadback final : public Device::Readback {
  public:
    BufferReadback(WGPUDevice device, WGPUQueue queue, WGPUBuffer source, std::size_t offset, std::size_t size);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    ReadbackBuffer copy;
};

} // namespace haylen::gpu::webgpu
