#pragma once

#include <span>

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The checks of images and their views against the contract and the capabilities of a device, which every device runs: the type, size, layers, mip levels, samples, format and kind of an image with the pixels of every level of a texture, the ranges of views, the writes into mip levels and layers of dynamic images, the images that generate their mip levels, and the parts of an image that a draw samples. A description that passes fits the device, so only a device that has no room for the image refuses it after.
class ImageRules final {
  public:
    // What a device keeps of an image or a view to check how it is used: its type, the size of its first mip level, its layers, mip levels, samples, format and kind, whether it is transient, and for a view the image it views with the first mip level and layer of its range there.
    struct Description {
        Device::TextureType type = Device::TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        Device::PixelFormat format = Device::PixelFormat::Rgba8;
        Device::ImageKind kind = Device::ImageKind::Texture;
        bool transient = false;
        Device::Image viewed{};
        int baseMipLevel = 0;
        int baseLayer = 0;

        [[nodiscard]] bool isView() const noexcept {
            return viewed.isValid();
        }

        [[nodiscard]] constexpr ImageInfo::Extent getLevelExtent(int level) const noexcept {
            return ImageInfo::getLevelExtent(type, width, height, layers, level);
        }
    };

    ImageRules() = delete;

    // Throws `ContractError` of the rule `Image` for a description that breaks the contract or reaches past the capabilities, and returns what a device keeps of it.
    [[nodiscard]] static Description check(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);

    // Throws `ContractError` of the rule `View` for a view that the device lacks, of an image that is a view, is multisampled or is transient, of another type than its image allows or of a range that the image does not hold, and returns what a device keeps of the view.
    [[nodiscard]] static Description checkView(const Description& image, const Device::ViewDesc& desc, const Device::Capabilities& capabilities);

    // Throws `ContractError` of the rule `ImageWrite` for a write into an image that is not dynamic, at a mip level or a layer it lacks, of a region outside that level or without a pixel, or with pixels of another size than the region.
    static void checkWrite(const Description& image, const Device::ImageWrite& write);

    // Throws `ContractError` of the rule `ImageWrite` when two writes of one call reach the same mip level and layer of an image.
    static void checkDistinctWrites(std::span<const Device::ImageWrite> writes);

    // Throws `ContractError` of the rule `Mipmaps` for an image that does not generate its mip levels: a view, a texture, a depth stencil image, an image of one mip level or of several samples, or one of a format whose `FormatSupport::mipmaps` the device lacks.
    static void checkMipmaps(const Description& image, const Device::Capabilities& capabilities);

    // Throws `ContractError` of the rule `Bindings` for a transient image, which no draw samples.
    static void checkSampled(const Description& image);

    // Throws `ContractError` of the rule `Readback` for a read of an image that is no render target of one sample, is a view or is transient, or of a region without a pixel or larger than the image.
    static void checkReadback(const Description& image, int width, int height);

    // Whether a draw that samples an image or a view, whose handle and description these are, reads the mip level and the layer of an image that a pass draws or resolves into. A view reads its own range alone, and an image reads every mip level and layer. The layer of a volume is a slice, which every view of the volume reads.
    [[nodiscard]] static bool reaches(Device::Image sampled, const Description& description, Device::Image image, int mipLevel, int layer) noexcept;

    // Whether a render target or a depth stencil image of the format takes the sample count, which is one of the bits of the `sampleCounts` of its `FormatSupport`.
    [[nodiscard]] static bool takesSamples(const Device::Capabilities& capabilities, Device::PixelFormat format, int sampleCount) noexcept;

  private:
    static void checkShape(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);
    static void checkKind(const Device::ImageDesc& desc, const Device::Capabilities& capabilities);
    static void checkLevels(const Device::ImageDesc& desc);
    static void checkViewLayers(const Description& image, const Device::ViewDesc& desc);
};

} // namespace haylen::gpu
