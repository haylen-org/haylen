#pragma once

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The check of the description of an image against the contract and the capabilities of a device, which every device runs before it makes an image: its type, size, layers, mip levels, samples, format and kind, and the pixels of every level of a texture. A description that passes fits the device, so only a device that has no room for the image refuses it after.
class ImageRules final {
  public:
    ImageRules() = delete;

    // Throws `ContractError` of the rule `Image` for a description that breaks the contract or reaches past the capabilities.
    static void check(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);

    // Whether a render target or a depth stencil image of the format takes the sample count, which is one of the bits of `Capabilities::sampleCounts`.
    [[nodiscard]] static bool takesSamples(const Device::Capabilities& capabilities, Device::PixelFormat format, int sampleCount) noexcept;

  private:
    static void checkShape(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);
    static void checkKind(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);
    static void checkLevels(const Device::ImageDesc& desc);
};

} // namespace haylen::gpu
