#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {

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

    // Returns the sampler that every texture with the options shares, creating it on first use.
    [[nodiscard]] sg_sampler getSampler(Texture::Options options);

    // Creates an image and the view that samples it.
    [[nodiscard]] static ImageViews createImage(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, const char* label);

    // Estimates the GPU memory an image of the size and pixel format takes, for the debug statistics.
    [[nodiscard]] static std::size_t getImageBytes(int width, int height, sg_pixel_format format);

    // Creates a render target of a pixel format, such as the floating-point light maps of the 2D renderer.
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, sg_pixel_format format, Texture::Options options);

  private:
    [[nodiscard]] static std::uint8_t samplerKey(Texture::Options options) noexcept;
    [[nodiscard]] static sg_wrap toWrap(Texture::Wrap wrap) noexcept;
};

} // namespace haylen::graphics
