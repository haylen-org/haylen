#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/d3d11/StagingCopy.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d11 {

// The copy of a range of a buffer into a staging buffer, whose bytes `poll` hands over as they are once the GPU finished the copy.
class BufferReadback final : public Device::Readback {
  public:
    BufferReadback(StagingCopy copy, std::size_t bytes);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    StagingCopy staging;
    std::size_t size = 0;
    bool delivered = false;
};

} // namespace haylen::gpu::d3d11
