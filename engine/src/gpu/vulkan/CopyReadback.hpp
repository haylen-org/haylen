#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gpu/vulkan/ReadbackQueue.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

// The readback of a render target or of a range of a buffer that the Vulkan device copies, which hands its bytes over once.
class CopyReadback final : public Device::Readback {
  public:
    explicit CopyReadback(std::shared_ptr<ReadbackQueue::Copy> started) noexcept : copy(std::move(started)) {}

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    std::shared_ptr<ReadbackQueue::Copy> copy;
};

} // namespace haylen::gpu::vulkan
