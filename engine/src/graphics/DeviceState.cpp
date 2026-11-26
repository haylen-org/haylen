#include "graphics/DeviceState.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <stdexcept>
#include <string_view>
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
    case Texture::Wrap::Border:
        return gpu::Device::Wrap::ClampToBorder;
    }

    return gpu::Device::Wrap::ClampToEdge;
}

gpu::Device::BorderColor DeviceState::toBorderColor(Texture::BorderColor color) noexcept {
    switch (color) {
    case Texture::BorderColor::TransparentBlack:
        return gpu::Device::BorderColor::TransparentBlack;
    case Texture::BorderColor::OpaqueBlack:
        return gpu::Device::BorderColor::OpaqueBlack;
    case Texture::BorderColor::OpaqueWhite:
        return gpu::Device::BorderColor::OpaqueWhite;
    }

    return gpu::Device::BorderColor::TransparentBlack;
}

// A sampler asked for without mipmaps, such as the one of a render target, reads only the image itself, since the textures that keep mipmaps settle them when they are made. The options reach here checked against the device, with their anisotropy settled.
gpu::Device::Sampler DeviceState::getSampler(Texture::Options options) {
    options.mipmaps = options.mipmaps.value_or(false);
    options.anisotropy = options.anisotropy.value_or(1);
    const auto found = std::ranges::find(samplers, options, &std::pair<Texture::Options, gpu::Device::Sampler>::first);
    if (found != samplers.end()) {
        return found->second;
    }

    const gpu::Device::Filter filter = options.filter == Texture::Filter::Nearest ? gpu::Device::Filter::Nearest : gpu::Device::Filter::Linear;
    const bool mipmaps = *options.mipmaps;
    const gpu::Device::Sampler sampler = gpu->makeSampler({
        .minification = filter,
        .magnification = filter,
        .mipmap = mipmaps ? filter : gpu::Device::Filter::Nearest,
        .wrapU = toWrap(options.wrapU),
        .wrapV = toWrap(options.wrapV),
        .wrapW = toWrap(options.wrapW),
        .minLod = options.minLod,
        .maxLod = options.maxLod,
        .lodBias = options.lodBias,
        .maxAnisotropy = options.anisotropy.value_or(1),
        .borderColor = toBorderColor(options.borderColor),
    });
    samplers.emplace_back(options, sampler);
    return sampler;
}

Texture::Options DeviceState::settle(Texture::Options options) const {
    const gpu::Device::Capabilities& capabilities = gpu->getCapabilities();
    const std::string_view api = std::ranges::find(gpu::Device::kApiNames, gpu->getApi(), &std::pair<std::string_view, gpu::Device::Api>::second)->first;
    const bool mipmaps = options.mipmaps.value_or(false);
    const bool linear = options.filter == Texture::Filter::Linear;
    options.anisotropy = options.anisotropy.value_or(linear && mipmaps ? capabilities.maxAnisotropy : 1);

    const int anisotropy = *options.anisotropy;
    if (anisotropy < 1 || anisotropy > capabilities.maxAnisotropy) {
        throw std::invalid_argument(std::format("The texture option \"anisotropy\" takes from 1 to {} samples on this device, not {}.", capabilities.maxAnisotropy, anisotropy));
    }

    if (anisotropy > 1 && !(linear && mipmaps)) {
        throw std::invalid_argument("Anisotropic filtering takes the filter \"linear\" and mipmaps.");
    }

    if (!(options.minLod >= 0.0F) || !(options.minLod <= options.maxLod)) {
        throw std::invalid_argument("The levels of detail of a texture run from \"minLod\" up to \"maxLod\", from 0 on.");
    }

    if (options.lodBias != 0.0F && capabilities.maxLodBias == 0.0F) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device takes no bias of the level of detail.", api));
    }

    if (!(std::abs(options.lodBias) <= capabilities.maxLodBias) && options.lodBias != 0.0F) {
        throw std::invalid_argument(std::format("The bias of the level of detail of a texture lies within {} on this device, not {}.", capabilities.maxLodBias, options.lodBias));
    }

    const bool border = options.wrapU == Texture::Wrap::Border || options.wrapV == Texture::Wrap::Border || options.wrapW == Texture::Wrap::Border;
    if (border && !capabilities.borderColors) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device reads no border color, so its textures take no wrap \"border\".", api));
    }

    return options;
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
    options.mipmaps = false;
    options = settle(options);
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
    stagePixels(resource, pixels, {0, 0, width, height});
    return Texture(std::move(resource));
}

void DeviceState::stagePixels(const std::shared_ptr<TextureResource>& resource, std::span<const std::uint8_t> pixels, gpu::Device::Region changed) {
    if (resource->pending) {
        changed = changed.merged(resource->stagedRegion);
    } else {
        resource->pending = true;
        changedTextures.push_back(resource);
    }

    const std::size_t pixelBytes = gpu::ImageInfo::getRowBytes(resource->format, 1);
    const std::size_t imageRow = gpu::ImageInfo::getRowBytes(resource->format, resource->width);
    const std::size_t regionRow = gpu::ImageInfo::getRowBytes(resource->format, changed.width);
    resource->stagedRegion = changed;
    resource->staged.resize(regionRow * static_cast<std::size_t>(changed.height));
    for (int row = 0; row < changed.height; ++row) {
        const std::size_t from = static_cast<std::size_t>(changed.y + row) * imageRow + static_cast<std::size_t>(changed.x) * pixelBytes;
        std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(from), regionRow, resource->staged.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * regionRow));
    }
}

std::size_t DeviceState::getImageBytes(int width, int height, gpu::Device::PixelFormat format) {
    return gpu::ImageInfo::getRegionBytes(format, width, height);
}

RenderTarget DeviceState::createRenderTarget(int width, int height, gpu::Device::PixelFormat format, Texture::Options options) {
    options.mipmaps = false;
    options = settle(options);
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
