#include "gpu/ImageRules.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

ImageRules::Description ImageRules::check(const Device::ImageDesc& desc, const Device::Capabilities& capabilities) {
    checkShape(desc, capabilities);
    checkKind(desc, capabilities);

    if (desc.kind == Device::ImageKind::Texture) {
        checkLevels(desc);
    }

    return {.type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .sampleCount = desc.sampleCount, .format = desc.format, .kind = desc.kind, .transient = desc.transient};
}

ImageRules::Description ImageRules::checkView(const Description& image, const Device::ViewDesc& desc, const Device::Capabilities& capabilities) {
    if (!capabilities.imageViews || image.isView() || image.sampleCount != 1 || image.transient) {
        throw ContractError(ContractError::Rule::View);
    }

    const bool faces = desc.type == Device::TextureType::Array && (image.type == Device::TextureType::Cube || image.type == Device::TextureType::CubeArray);
    if (desc.type != image.type && !faces) {
        throw ContractError(ContractError::Rule::View);
    }

    if (desc.baseMipLevel < 0 || desc.mipLevels < 1 || desc.baseMipLevel + desc.mipLevels > image.mipLevels) {
        throw ContractError(ContractError::Rule::View);
    }

    checkViewLayers(image, desc);

    const ImageInfo::Extent base = image.getLevelExtent(desc.baseMipLevel);
    const int layers = desc.type == Device::TextureType::Volume ? base.depth : desc.layers;
    return {.type = desc.type, .width = base.width, .height = base.height, .layers = layers, .mipLevels = desc.mipLevels, .sampleCount = 1, .format = image.format, .kind = image.kind, .viewed = desc.image, .baseMipLevel = desc.baseMipLevel, .baseLayer = desc.baseLayer};
}

// A flat view takes the one layer of its image, a cube view six faces, a cube array view whole cubes and a volume view every slice, while an array view takes any layers its image holds.
void ImageRules::checkViewLayers(const Description& image, const Device::ViewDesc& desc) {
    const bool inside = desc.baseLayer >= 0 && desc.layers >= 1 && desc.baseLayer + desc.layers <= image.layers;
    bool fits = inside;
    switch (desc.type) {
    case Device::TextureType::Flat:
        fits = desc.baseLayer == 0 && desc.layers == 1;
        break;
    case Device::TextureType::Array:
        break;
    case Device::TextureType::Volume:
        fits = desc.baseLayer == 0 && desc.layers == image.layers;
        break;
    case Device::TextureType::Cube:
        fits = desc.baseLayer == 0 && desc.layers == 6;
        break;
    case Device::TextureType::CubeArray:
        fits = inside && desc.baseLayer % 6 == 0 && desc.layers % 6 == 0;
        break;
    }

    if (!fits) {
        throw ContractError(ContractError::Rule::View);
    }
}

void ImageRules::checkWrite(const Description& image, const Device::ImageWrite& write) {
    if (image.isView() || image.kind != Device::ImageKind::Dynamic || write.mipLevel < 0 || write.mipLevel >= image.mipLevels) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    const ImageInfo::Extent level = image.getLevelExtent(write.mipLevel);
    const Device::Region& region = write.region;
    const bool inside = region.x >= 0 && region.y >= 0 && region.width > 0 && region.height > 0 && region.x + region.width <= level.width && region.y + region.height <= level.height;
    if (!inside || write.layer < 0 || write.layer >= level.depth) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    if (write.pixels.size() != ImageInfo::getRegionBytes(image.format, region.width, region.height)) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }
}

void ImageRules::checkDistinctWrites(std::span<const Device::ImageWrite> writes) {
    for (auto write = writes.begin(); write != writes.end(); ++write) {
        const bool repeated = std::any_of(writes.begin(), write, [&write](const Device::ImageWrite& earlier) { return earlier.image == write->image && earlier.mipLevel == write->mipLevel && earlier.layer == write->layer; });
        if (repeated) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }
    }
}

void ImageRules::checkMipmaps(const Description& image, const Device::Capabilities& capabilities) {
    const bool generates = image.kind == Device::ImageKind::Dynamic || image.kind == Device::ImageKind::RenderTarget;
    if (image.isView() || !generates || image.mipLevels < 2 || image.sampleCount != 1 || !capabilities.getFormat(image.format).mipmaps) {
        throw ContractError(ContractError::Rule::Mipmaps);
    }
}

void ImageRules::checkSampled(const Description& image) {
    if (image.transient) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

void ImageRules::checkReadback(const Description& image, int width, int height) {
    if (image.isView() || image.kind != Device::ImageKind::RenderTarget || image.sampleCount != 1 || image.transient) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (width < 1 || height < 1 || width > image.width || height > image.height) {
        throw ContractError(ContractError::Rule::Readback);
    }
}

bool ImageRules::reaches(Device::Image sampled, const Description& description, Device::Image image, int mipLevel, int layer) noexcept {
    const Device::Image viewed = description.isView() ? description.viewed : sampled;
    if (viewed != image || mipLevel < description.baseMipLevel || mipLevel >= description.baseMipLevel + description.mipLevels) {
        return false;
    }

    return description.type == Device::TextureType::Volume || (layer >= description.baseLayer && layer < description.baseLayer + description.layers);
}

bool ImageRules::takesSamples(const Device::Capabilities& capabilities, Device::PixelFormat format, int sampleCount) noexcept {
    if (sampleCount < 1 || sampleCount > 128 || !std::has_single_bit(static_cast<unsigned>(sampleCount))) {
        return false;
    }

    return (capabilities.getFormat(format).sampleCounts & static_cast<unsigned>(sampleCount)) != 0;
}

// A flat image and the layers of an array take the largest image, a volume the largest volume along its width, height and slices, a cube six square faces of the largest cube, and a cube array on a device that has them six such faces for each of its cubes within the most layers of an array.
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
    case Device::TextureType::CubeArray:
        largest = capabilities.maxCubeSize;
        layers = capabilities.cubeArrays && desc.layers >= 6 && desc.layers % 6 == 0 && desc.layers <= capabilities.maxArrayLayers && desc.width == desc.height;
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

// Depth formats belong to depth stencil images alone, which are no volume since no graphics API has depth volumes, block compressed formats to textures that are no volume and whose sides hold whole blocks, only textures come with pixels, only attachments take samples and are transient, a transient image is flat with one mip level and one layer, as the memory of the tiles of a pass holds it, and the format is one the device makes images of that kind in.
void ImageRules::checkKind(const Device::ImageDesc& desc, const Device::Capabilities& capabilities) {
    if (ImageInfo::isDepth(desc.format) != (desc.kind == Device::ImageKind::DepthStencil)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.kind == Device::ImageKind::DepthStencil && desc.type == Device::TextureType::Volume) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (ImageInfo::isCompressed(desc.format)) {
        const ImageInfo::Block block = ImageInfo::getBlock(desc.format);
        if (desc.kind != Device::ImageKind::Texture || desc.type == Device::TextureType::Volume || desc.width % block.width != 0 || desc.height % block.height != 0) {
            throw ContractError(ContractError::Rule::Image);
        }
    }

    if (desc.kind != Device::ImageKind::Texture && !desc.levels.empty()) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.sampleCount > 1 && (desc.kind == Device::ImageKind::Texture || desc.kind == Device::ImageKind::Dynamic)) {
        throw ContractError(ContractError::Rule::Image);
    }

    const bool attachment = desc.kind == Device::ImageKind::RenderTarget || desc.kind == Device::ImageKind::DepthStencil;
    if (desc.transient && (!attachment || desc.type != Device::TextureType::Flat || desc.mipLevels != 1)) {
        throw ContractError(ContractError::Rule::Image);
    }

    const Device::FormatSupport& support = capabilities.getFormat(desc.format);
    if (!(attachment ? support.render : support.sample)) {
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
