#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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
    std::unordered_map<std::uint8_t, gpu::Device::Sampler> samplers;
    Texture white;
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
        std::unique_ptr<gpu::Device::Readback> copy;
    };

    std::vector<PendingRead> reads;

    // Returns the sampler that every texture with the options shares, creating it on first use.
    [[nodiscard]] gpu::Device::Sampler getSampler(Texture::Options options);

    // Creates the image of a texture, with its mipmaps when asked for, and counts the bytes they hold.
    [[nodiscard]] gpu::Device::Image createTextureImage(const Image& image, bool mipmaps, std::size_t& bytes);

    // Creates an image whose pixels change in place. Its pixels arrive with the next upload.
    [[nodiscard]] gpu::Device::Image createDynamicImage(int width, int height, gpu::Device::PixelFormat format);

    // Returns the bytes an image of the size and pixel format holds, for the debug statistics.
    [[nodiscard]] static std::size_t getImageBytes(int width, int height, gpu::Device::PixelFormat format);

    // Creates a render target of a pixel format, such as the floating-point light maps of the 2D renderer.
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, gpu::Device::PixelFormat format, Texture::Options options);

    // Creates a texture whose pixels change in place, and keeps its first pixels for the next upload.
    [[nodiscard]] Texture createDynamicTexture(int width, int height, gpu::Device::PixelFormat format, std::span<const std::uint8_t> pixels, Texture::Options options);

    // Keeps the pixels of a dynamic texture for the next upload, replacing the ones it kept before.
    void stagePixels(const std::shared_ptr<TextureResource>& resource, std::span<const std::uint8_t> pixels);

  private:
    [[nodiscard]] static std::uint8_t samplerKey(Texture::Options options) noexcept;
    [[nodiscard]] static gpu::Device::Wrap toWrap(Texture::Wrap wrap) noexcept;
};

} // namespace haylen::graphics
