#include "gpu/direct3d/MipLevels.hpp"

#include <algorithm>

#include "gpu/ImageInfo.hpp"

namespace haylen::gpu::direct3d {

MipLevels::Level MipLevels::describe(Device::TextureType type, int width, int height, int layers, Device::PixelFormat format, int level) noexcept {
    Level described;
    described.width = static_cast<std::size_t>(std::max(1, width >> level));
    described.height = static_cast<std::size_t>(std::max(1, height >> level));
    described.layers = static_cast<std::size_t>(type == Device::TextureType::Volume ? std::max(1, layers >> level) : layers);
    described.rows = static_cast<std::size_t>(ImageInfo::getRowCount(format, static_cast<int>(described.height)));
    described.rowBytes = ImageInfo::getRowBytes(format, static_cast<int>(described.width));
    described.sliceBytes = described.rowBytes * described.rows;
    return described;
}

} // namespace haylen::gpu::direct3d
