#include "gpu/direct3d/MipLevels.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "gpu/direct3d/Formats.hpp"
#include "haylen/gpu/ContractError.hpp"

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

void MipLevels::check(const Device::ImageDesc& desc) {
    if (desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw ContractError(ContractError::Rule::Image);
    }

    for (std::size_t index = 0; index < desc.levels.size(); ++index) {
        const Level level = describe(desc.type, desc.width, desc.height, desc.layers, desc.format, static_cast<int>(index));
        if (desc.levels[index].size() != level.sliceBytes * level.layers) {
            throw ContractError(ContractError::Rule::Image);
        }
    }
}

} // namespace haylen::gpu::direct3d
