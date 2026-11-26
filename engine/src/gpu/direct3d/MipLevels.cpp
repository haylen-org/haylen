#include "gpu/direct3d/MipLevels.hpp"

#include <algorithm>

#include "gpu/direct3d/Formats.hpp"

namespace haylen::gpu::direct3d {

MipLevels::Level MipLevels::describe(Device::TextureType type, int width, int height, int layers, Device::PixelFormat format, int level) noexcept {
    Level described;
    described.width = static_cast<std::size_t>(std::max(1, width >> level));
    described.height = static_cast<std::size_t>(std::max(1, height >> level));
    described.layers = static_cast<std::size_t>(type == Device::TextureType::Volume ? std::max(1, layers >> level) : layers);
    described.rowBytes = described.width * Formats::getBytesPerPixel(format);
    described.sliceBytes = described.rowBytes * described.height;
    return described;
}

} // namespace haylen::gpu::direct3d
