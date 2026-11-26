#include "gpu/PassRules.hpp"

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

void PassRules::checkCount(const Device::PassDesc& desc) {
    if (desc.colors.size() > Device::kMaxColorTargets) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }
}

void PassRules::checkColor(const Device::ColorAttachment& color, const ImageRules::Description& image, const ImageRules::Description* resolve, Extent& extent) {
    if (image.isView() || image.kind != Device::ImageKind::RenderTarget) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    checkLevel(image, color.mipLevel, color.layer, extent);

    if (image.transient && keeps(color.load, color.store)) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (color.resolve.isValid() != (resolve != nullptr)) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (resolve == nullptr) {
        return;
    }

    if (resolve->kind != Device::ImageKind::RenderTarget || resolve->type == Device::TextureType::Volume) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    checkResolve(image, color.mipLevel, *resolve, color.resolveMipLevel, color.resolveLayer);
}

void PassRules::checkDepthStencil(const Device::DepthStencilAttachment& depthStencil, const ImageRules::Description& image, const ImageRules::Description* resolve, const Device::Capabilities& capabilities, Extent& extent) {
    if (image.isView() || image.kind != Device::ImageKind::DepthStencil) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    checkLevel(image, depthStencil.mipLevel, depthStencil.layer, extent);

    if (depthStencil.layer > 0 && !capabilities.depthLayers) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (image.transient && (keeps(depthStencil.depthLoad, depthStencil.depthStore) || keeps(depthStencil.stencilLoad, depthStencil.stencilStore))) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (depthStencil.resolve.isValid() != (resolve != nullptr)) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (resolve == nullptr) {
        return;
    }

    if (!capabilities.hasDepthResolve(depthStencil.resolveMode) || ImageInfo::hasStencil(image.format) || resolve->kind != Device::ImageKind::DepthStencil) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    checkResolve(image, depthStencil.mipLevel, *resolve, depthStencil.resolveMipLevel, depthStencil.resolveLayer);
}

// Every attachment of a pass has the size of the first one at its mip level and its sample count.
void PassRules::checkLevel(const ImageRules::Description& image, int mipLevel, int layer, Extent& extent) {
    if (mipLevel < 0 || mipLevel >= image.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent level = image.getLevelExtent(mipLevel);
    if (layer < 0 || layer >= level.depth) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (extent.width == 0) {
        extent = {.width = level.width, .height = level.height, .sampleCount = image.sampleCount};
    }

    if (level.width != extent.width || level.height != extent.height || image.sampleCount != extent.sampleCount) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }
}

// A multisampled attachment resolves into a mip level and a layer of an image of one sample of its format and of its size there, which is neither a view nor transient.
void PassRules::checkResolve(const ImageRules::Description& image, int mipLevel, const ImageRules::Description& resolve, int resolveMipLevel, int resolveLayer) {
    if (image.sampleCount == 1 || resolve.isView() || resolve.transient || resolve.sampleCount != 1 || resolve.format != image.format) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (resolveMipLevel < 0 || resolveMipLevel >= resolve.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent level = image.getLevelExtent(mipLevel);
    const ImageInfo::Extent resolved = resolve.getLevelExtent(resolveMipLevel);
    if (resolveLayer < 0 || resolveLayer >= resolved.depth || resolved.width != level.width || resolved.height != level.height) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }
}

// A transient image holds nothing before its pass and nothing after it, so it clears or leaves its contents undefined and discards them.
bool PassRules::keeps(Device::Load load, Device::Store store) noexcept {
    return load == Device::Load::Keep || store == Device::Store::Store;
}

} // namespace haylen::gpu
