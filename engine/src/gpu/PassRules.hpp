#pragma once

#include "gpu/ImageRules.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The checks of the attachments of an offscreen pass against the contract and the capabilities of a device, which every device runs before it begins the pass: images of their kind at a mip level and a layer they hold, one size and one sample count for every attachment, loads and stores that keep nothing of a transient image, and resolves into images of one sample of the same format and size.
class PassRules final {
  public:
    // The size of the first attachment of a pass at its mip level and its sample count, which every other attachment of the pass takes.
    struct Extent {
        int width = 0;
        int height = 0;
        int sampleCount = 1;
    };

    PassRules() = delete;

    // Throws `ContractError` of the rule `PassAttachments` for more color attachments than `Device::kMaxColorTargets` or an attachment that breaks the rules of `checkColor` and `checkDepthStencil`, and returns the size and the sample count of the pass. The function `find` takes an image of the pass and returns its description.
    template <typename Find> [[nodiscard]] static Extent check(const Device::PassDesc& desc, const Device::Capabilities& capabilities, const Find& find) {
        checkCount(desc);

        Extent extent;
        for (const Device::ColorAttachment& color : desc.colors) {
            checkColor(color, find(color.image), color.resolve.isValid() ? &find(color.resolve) : nullptr, extent);
        }

        if (desc.depthStencil) {
            const Device::DepthStencilAttachment& depthStencil = *desc.depthStencil;
            checkDepthStencil(depthStencil, find(depthStencil.image), depthStencil.resolve.isValid() ? &find(depthStencil.resolve) : nullptr, capabilities, extent);
        }

        return extent;
    }

  private:
    // Throws `ContractError` of the rule `PassAttachments` for more color attachments than `Device::kMaxColorTargets`.
    static void checkCount(const Device::PassDesc& desc);

    // Throws `ContractError` of the rule `PassAttachments` for a color attachment that is no render target at a mip level and a layer it holds, of another size or sample count than the pass, a transient image that loads or stores, or a resolve that is no render target of one sample of the same format and size at its mip level and layer, a volume, a view or a transient image. The description of the resolve image comes when the attachment names one.
    static void checkColor(const Device::ColorAttachment& color, const ImageRules::Description& image, const ImageRules::Description* resolve, Extent& extent);

    // Throws `ContractError` of the rule `PassAttachments` for a depth stencil attachment that is no depth stencil image at a mip level and a layer it holds, at a layer after the first on a device without `depthLayers`, of another size or sample count than the pass, a transient image that loads or stores, or a depth resolve of a mode the device lacks, of an image of one sample or of a format with stencil, into an image that is no depth stencil image of one sample of the same format and size at its mip level and layer, a view or a transient image. The description of the resolve image comes when the attachment names one.
    static void checkDepthStencil(const Device::DepthStencilAttachment& depthStencil, const ImageRules::Description& image, const ImageRules::Description* resolve, const Device::Capabilities& capabilities, Extent& extent);

    static void checkLevel(const ImageRules::Description& image, int mipLevel, int layer, Extent& extent);
    static void checkResolve(const ImageRules::Description& image, int mipLevel, const ImageRules::Description& resolve, int resolveMipLevel, int resolveLayer);
    [[nodiscard]] static bool keeps(Device::Load load, Device::Store store) noexcept;
};

} // namespace haylen::gpu
