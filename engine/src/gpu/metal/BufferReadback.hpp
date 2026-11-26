#pragma once

#import <Metal/Metal.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/metal/ReadbackCopy.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::metal {

// A copy of a range of a buffer into a buffer the CPU reads, after the frames committed before it, whose bytes come as they are.
class BufferReadback final : public gpu::Device::Readback {
  public:
    BufferReadback(id<MTLCommandQueue> queue, id<MTLBuffer> source, std::size_t offset, std::size_t size);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    ReadbackCopy copy;
};

} // namespace haylen::gpu::metal
