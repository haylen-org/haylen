#include "haylen/graphics/Buffer.hpp"

#include <algorithm>

#include "graphics/BufferResource.hpp"

namespace haylen::graphics {

bool Buffer::addUsage(Usage& usage, std::string_view name) noexcept {
    const auto found = std::ranges::find(kUsageNames, name, &std::pair<std::string_view, bool Usage::*>::first);
    if (found == kUsageNames.end()) {
        return false;
    }

    usage.*(found->second) = true;
    return true;
}

std::size_t Buffer::getSize() const noexcept {
    return resource ? resource->size : 0;
}

Buffer::Usage Buffer::getUsage() const noexcept {
    return resource ? resource->usage : Usage{};
}

Buffer::Update Buffer::getUpdate() const noexcept {
    return resource ? resource->update : Update::Immutable;
}

std::string_view Buffer::getLabel() const noexcept {
    return resource ? std::string_view(resource->label) : std::string_view{};
}

gpu::Device::Buffer Buffer::getGpuBuffer() const noexcept {
    return resource ? resource->buffer : gpu::Device::Buffer{};
}

} // namespace haylen::graphics
