#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {

struct TextureResource;

// The GPU state of a device, which the renderers and the resources it creates share.
struct DeviceState {
    struct ImageViews {
        sg_image image{};
        sg_view view{};
    };

    std::shared_ptr<ResourceGraveyard> graveyard = std::make_shared<ResourceGraveyard>();
    std::unordered_map<std::uint8_t, sg_sampler> samplers;
    Texture white;
    std::uint32_t nextTextureId = 1;
    sg_pixel_format offscreenFormat = SG_PIXELFORMAT_RGBA8;

    // The dynamic textures whose pixels changed since the last upload.
    std::vector<std::weak_ptr<TextureResource>> changedTextures;

    // Returns the sampler that every texture with the options shares, creating it on first use.
    [[nodiscard]] sg_sampler getSampler(Texture::Options options);

    // Creates an image and the view that samples it.
    [[nodiscard]] static ImageViews createImage(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, const char* label);

    // Creates an image whose pixels change in place and the view that samples it. Its pixels arrive with the next upload.
    [[nodiscard]] static ImageViews createDynamicImage(int width, int height, sg_pixel_format format);

    // Estimates the GPU memory an image of the size and pixel format takes, for the debug statistics.
    [[nodiscard]] static std::size_t getImageBytes(int width, int height, sg_pixel_format format);

    // Creates a render target of a pixel format, such as the floating-point light maps of the 2D renderer.
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, sg_pixel_format format, Texture::Options options);

    // Creates a texture whose pixels change in place, and keeps its first pixels for the next upload.
    [[nodiscard]] Texture createDynamicTexture(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, Texture::Options options);

    // Keeps the pixels of a dynamic texture for the next upload, replacing the ones it kept before.
    void stagePixels(const std::shared_ptr<TextureResource>& resource, std::span<const std::uint8_t> pixels);

  private:
    [[nodiscard]] static std::uint8_t samplerKey(Texture::Options options) noexcept;
    [[nodiscard]] static sg_wrap toWrap(Texture::Wrap wrap) noexcept;
};

} // namespace haylen::graphics
