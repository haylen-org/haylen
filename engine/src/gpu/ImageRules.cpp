#include "gpu/ImageRules.hpp"

#include <array>
#include <bit>
#include <cstddef>

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

void ImageRules::check(const Device::ImageDesc& desc, const Device::Capabilities& capabilities) {
    checkShape(desc, capabilities);
    checkKind(desc, capabilities);

    if (desc.kind == Device::ImageKind::Texture) {
        checkLevels(desc);
    }
}

bool ImageRules::takesSamples(const Device::Capabilities& capabilities, Device::PixelFormat format, int sampleCount) noexcept {
    if (sampleCount < 1 || sampleCount > 128 || !std::has_single_bit(static_cast<unsigned>(sampleCount))) {
        return false;
    }

    return (capabilities.sampleCounts[static_cast<std::size_t>(format)] & static_cast<unsigned>(sampleCount)) != 0;
}

// A flat image and the layers of an array take the largest image, a volume the largest volume along its width, height and slices, and a cube six square faces of the largest cube.
void ImageRules::checkShape(const Device::ImageDesc& desc, const Device::Capabilities& capabilities) {
    int largest = capabilities.maxImageSize;
    bool layers = desc.layers == 1;
    switch (desc.type) {
    case Device::TextureType::Flat:
        break;
    case Device::TextureType::Array:
        layers = desc.layers >= 1 && desc.layers <= capabilities.maxArrayLayers;
        break;
    case Device::TextureType::Volume:
        largest = capabilities.maxVolumeSize;
        layers = desc.layers >= 1 && desc.layers <= capabilities.maxVolumeSize;
        break;
    case Device::TextureType::Cube:
        largest = capabilities.maxCubeSize;
        layers = desc.layers == 6 && desc.width == desc.height;
        break;
    }

    if (desc.width < 1 || desc.height < 1 || desc.width > largest || desc.height > largest || !layers) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.mipLevels < 1 || desc.mipLevels > ImageInfo::getMaxMipLevels(desc)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.sampleCount != 1 && (desc.type != Device::TextureType::Flat || desc.mipLevels != 1 || !takesSamples(capabilities, desc.format, desc.sampleCount))) {
        throw ContractError(ContractError::Rule::Image);
    }
}

// Depth formats belong to depth stencil images alone, only textures come with pixels, only attachments take samples, a dynamic image is flat with one level, and the format is one the device makes images of that kind in.
void ImageRules::checkKind(const Device::ImageDesc& desc, const Device::Capabilities& capabilities) {
    if (ImageInfo::isDepth(desc.format) != (desc.kind == Device::ImageKind::DepthStencil)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.kind != Device::ImageKind::Texture && !desc.levels.empty()) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.sampleCount > 1 && (desc.kind == Device::ImageKind::Texture || desc.kind == Device::ImageKind::Dynamic)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.kind == Device::ImageKind::Dynamic && (desc.mipLevels != 1 || desc.type != Device::TextureType::Flat)) {
        throw ContractError(ContractError::Rule::Image);
    }

    const bool renders = desc.kind == Device::ImageKind::RenderTarget || desc.kind == Device::ImageKind::DepthStencil;
    const std::array<bool, 16>& supported = renders ? capabilities.renderFormats : capabilities.textureFormats;
    if (!supported[static_cast<std::size_t>(desc.format)]) {
        throw ContractError(ContractError::Rule::Image);
    }
}

void ImageRules::checkLevels(const Device::ImageDesc& desc) {
    if (desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw ContractError(ContractError::Rule::Image);
    }

    for (std::size_t level = 0; level < desc.levels.size(); ++level) {
        if (desc.levels[level].size() != ImageInfo::getLevelBytes(desc, static_cast<int>(level))) {
            throw ContractError(ContractError::Rule::Image);
        }
    }
}

} // namespace haylen::gpu
