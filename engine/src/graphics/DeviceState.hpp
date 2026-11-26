#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

struct TextureResource;

// The GPU state of a device, which the renderers and the resources it creates share.
struct DeviceState {
    explicit DeviceState(std::unique_ptr<gpu::Device> device) : gpu(std::move(device)) {}

    std::unique_ptr<gpu::Device> gpu;
    std::shared_ptr<ResourceGraveyard> graveyard = std::make_shared<ResourceGraveyard>();
    std::vector<std::pair<Texture::Options, gpu::Device::Sampler>> samplers;
    Texture white;

    // The white textures of the other kinds, by `gpu::Device::TextureType`, made when a material first samples one.
    std::array<Texture, gpu::Device::kTextureTypeNames.size()> whites;
    std::uint32_t nextTextureId = 1;
    gpu::Device::PixelFormat offscreenFormat = gpu::Device::PixelFormat::Rgba8;

    // The dynamic textures whose pixels changed since the last upload, the ones the last upload sent, the list an upload walks and the writes it hands the device, which the uploads reuse so none of them allocates once they have grown.
    std::vector<std::weak_ptr<TextureResource>> changedTextures;
    std::vector<std::weak_ptr<TextureResource>> uploadedTextures;
    std::vector<std::weak_ptr<TextureResource>> uploadingTextures;
    std::vector<std::shared_ptr<TextureResource>> uploading;
    std::vector<gpu::Device::ImageWrite> writes;
    std::uint64_t uploads = 0;

    // A render target whose pixels a caller asked for, and the copy of them once the frame that asked was submitted.
    struct PendingRead {
        RenderTarget target;
        Device::ReadCallback done;
        std::unique_ptr<gpu::Device::Readback> copy{};
    };

    std::vector<PendingRead> reads;

    // A range of a buffer whose bytes a caller asked for, and the copy of them once the frame that asked was submitted.
    struct PendingBufferRead {
        Buffer buffer;
        std::size_t offset = 0;
        std::size_t size = 0;
        Device::BufferReadCallback done;
        std::unique_ptr<gpu::Device::Readback> copy{};
    };

    std::vector<PendingBufferRead> bufferReads;

    // The capture that records, with the frames it still records and the callback that receives its file.
    struct PendingCapture {
        int framesLeft = 0;
        Device::CaptureCallback done;
    };

    std::optional<PendingCapture> capture;

    // The memory that the GPU device held for later at the last frame, and the seconds of frames since it last grew, while a trim waits for it to stop growing.
    std::size_t spare = 0;
    std::optional<double> sinceSpareGrew;

    // Returns the sampler that every texture with the options shares, creating it on first use.
    [[nodiscard]] gpu::Device::Sampler getSampler(Texture::Options options);

    // Checks the sampling of the options against the device, with their mipmaps settled, and gives a texture with the linear filter and mipmaps the most anisotropy of the device unless the options name one. Throws `std::invalid_argument` with a sentence for options that the device cannot sample with.
    [[nodiscard]] Texture::Options settle(Texture::Options options) const;

    // Creates the image of a texture, with its mipmaps when asked for, and counts the bytes and the mip levels they hold.
    [[nodiscard]] gpu::Device::Image createTextureImage(const Image& image, bool mipmaps, std::size_t& bytes, int& mipLevels);

    // The layers of the image of texture data as the GPU device counts them: the slices of a volume, and the layers or faces of the other types.
    [[nodiscard]] static int getLayers(const Device::TextureData& data) noexcept;

    // Returns the bytes an image of the size and pixel format holds, for the debug statistics.
    [[nodiscard]] static std::size_t getImageBytes(int width, int height, gpu::Device::PixelFormat format);

    // Creates a render target of a pixel format, such as the floating-point light maps of the 2D renderer.
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, gpu::Device::PixelFormat format, Texture::Options options);

    // Creates a texture whose image takes the pixels of the first level of each layer with the next upload, which keeps them for later uploads when the texture is dynamic. With `mipmaps` the image holds every mip level, which the GPU generates after each upload.
    [[nodiscard]] Texture createStagedTexture(const Device::TextureData& data, Texture::Options options, bool dynamic);

    // Makes the image of a staged texture of the shape, with every mip level when the GPU generates them.
    [[nodiscard]] gpu::Device::Image createStagedImage(const Device::TextureData& data, bool mipmaps, int& mipLevels);

    // Keeps the pixels of the region of a layer of a staged texture that changed for the next upload, out of every pixel of the layer. A layer that already waits for an upload keeps the smallest region that holds both changes, from the newer pixels.
    void stagePixels(const std::shared_ptr<TextureResource>& resource, int layer, std::span<const std::uint8_t> pixels, gpu::Device::Region changed);

  private:
    // The samples a linear texture with mipmaps takes along the longer side of the footprint of a pixel, so a texture drawn much smaller along one side than the other, such as the middle of a nine-slice squeezed into a narrow button, keeps the detail of its other side.

    [[nodiscard]] static gpu::Device::BorderColor toBorderColor(Texture::BorderColor color) noexcept;
    [[nodiscard]] static gpu::Device::Wrap toWrap(Texture::Wrap wrap) noexcept;
};

} // namespace haylen::graphics
