#include "gpu/vulkan/ImageReadback.hpp"

#include <stdexcept>

namespace haylen::gpu::vulkan {

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (copy->queue != nullptr) {
        copy->queue->update(*copy);
    }
    if (!copy->failure.empty()) {
        throw std::runtime_error(copy->failure);
    }
    return std::exchange(copy->pixels, std::nullopt);
}

} // namespace haylen::gpu::vulkan
