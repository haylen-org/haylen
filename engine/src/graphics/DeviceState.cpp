#include "graphics/DeviceState.hpp"

#include <memory>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "graphics/RenderTargetResource.hpp"
#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

gpu::Device::Wrap DeviceState::toWrap(Texture::Wrap wrap) noexcept {
    switch (wrap) {
    case Texture::Wrap::Clamp:
        return gpu::Device::Wrap::ClampToEdge;
    case Texture::Wrap::Repeat:
        return gpu::Device::Wrap::Repeat;
    case Texture::Wrap::Mirror:
        return gpu::Device::Wrap::MirroredRepeat;
    }
    return gpu::Device::Wrap::ClampToEdge;
}

std::uint8_t DeviceState::samplerKey(Texture::Options options) noexcept {
    return static_cast<std::uint8_t>(static_cast<unsigned>(options.filter) | (static_cast<unsigned>(options.wrap) << 1U) | (options.mipmaps ? 8U : 0U));
}

gpu::Device::Sampler DeviceState::getSampler(Texture::Options options) {
    const std::uint8_t key = samplerKey(options);
    if (const auto found = samplers.find(key); found != samplers.end()) {
        return found->second;
    }
    const gpu::Device::Filter filter = options.filter == Texture::Filter::Nearest ? gpu::Device::Filter::Nearest : gpu::Device::Filter::Linear;
    const gpu::Device::Sampler sampler = gpu->makeSampler({.minification = filter, .magnification = filter, .mipmap = options.mipmaps ? filter : gpu::Device::Filter::Nearest, .wrapU = toWrap(options.wrap), .wrapV = toWrap(options.wrap)});
    samplers.emplace(key, sampler);
    return sampler;
}

// Mipmaps an image brings from its decoding are taken as they are, and the others are made here.
gpu::Device::Image DeviceState::createTextureImage(const Image& image, bool mipmaps, std::size_t& bytes) {
    bytes = getImageBytes(image.getWidth(), image.getHeight(), gpu::Device::PixelFormat::Rgba8);
    Image levels;
    std::span<const Image> made;
    if (mipmaps) {
        made = image.getMipmaps();
        if (made.empty() && (image.getWidth() > 1 || image.getHeight() > 1)) {
            levels = image;
            levels.buildMipmaps();
            made = levels.getMipmaps();
        }
    }

    std::vector<std::span<const std::byte>> pixels;
    pixels.reserve(made.size() + 1);
    pixels.push_back(std::as_bytes(image.getPixels()));
    for (const Image& level : made) {
        pixels.push_back(std::as_bytes(level.getPixels()));
        bytes += level.getPixels().size();
    }
    return gpu->makeImage({.width = image.getWidth(), .height = image.getHeight(), .mipLevels = static_cast<int>(pixels.size()), .format = gpu::Device::PixelFormat::Rgba8, .kind = gpu::Device::ImageKind::Texture, .levels = pixels, .label = "haylen-texture"});
}

gpu::Device::Image DeviceState::createDynamicImage(int width, int height, gpu::Device::PixelFormat format) {
    return gpu->makeImage({.width = width, .height = height, .format = format, .kind = gpu::Device::ImageKind::Dynamic, .label = "haylen-dynamic-texture"});
}

Texture DeviceState::createDynamicTexture(int width, int height, gpu::Device::PixelFormat format, std::span<const std::uint8_t> pixels, Texture::Options options) {
    auto resource = std::make_shared<TextureResource>();
    resource->image = createDynamicImage(width, height, format);
    resource->tracked.setBytes(getImageBytes(width, height, format));
    resource->sampler = getSampler(options);
    resource->width = width;
    resource->height = height;
    resource->id = nextTextureId++;
    resource->options = options;
    resource->format = format;
    resource->dynamic = true;
    resource->graveyard = graveyard;
    stagePixels(resource, pixels);
    return Texture(std::move(resource));
}

void DeviceState::stagePixels(const std::shared_ptr<TextureResource>& resource, std::span<const std::uint8_t> pixels) {
    if (!resource->pending) {
        resource->pending = true;
        changedTextures.push_back(resource);
    }
    resource->staged.assign(pixels.begin(), pixels.end());
}

std::size_t DeviceState::getImageBytes(int width, int height, gpu::Device::PixelFormat format) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * gpu::ImageInfo::getBytesPerPixel(format);
}

RenderTarget DeviceState::createRenderTarget(int width, int height, gpu::Device::PixelFormat format, Texture::Options options) {
    auto texture = std::make_shared<TextureResource>(TextureResource::targets);
    texture->image = gpu->makeImage({.width = width, .height = height, .format = format, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-render-target"});
    texture->tracked.setBytes(getImageBytes(width, height, format));
    texture->sampler = getSampler(options);
    texture->width = width;
    texture->height = height;
    texture->id = nextTextureId++;
    texture->options = options;
    texture->format = format;
    texture->flipped = !gpu->getCapabilities().originTopLeft;
    texture->graveyard = graveyard;

    auto resource = std::make_shared<RenderTargetResource>();
    resource->texture = Texture(std::move(texture));
    return RenderTarget(std::move(resource));
}

} // namespace haylen::graphics
