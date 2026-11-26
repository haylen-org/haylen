#include "gpu/vulkan/CopyReadback.hpp"

#include <stdexcept>

namespace haylen::gpu::vulkan {

std::optional<std::vector<std::uint8_t>> CopyReadback::poll() {
    if (copy->queue != nullptr) {
        copy->queue->update(*copy);
    }

    if (!copy->failure.empty()) {
        throw std::runtime_error(copy->failure);
    }

    return std::exchange(copy->bytes, std::nullopt);
}

} // namespace haylen::gpu::vulkan
