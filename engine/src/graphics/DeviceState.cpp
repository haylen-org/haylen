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
gpu::Device::Image DeviceState::createTextureImage(const Image& image, bool mipmaps, std::size_t& bytes, int& mipLevels) {
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

    mipLevels = static_cast<int>(pixels.size());
    return gpu->makeImage({.width = image.getWidth(), .height = image.getHeight(), .mipLevels = mipLevels, .format = gpu::Device::PixelFormat::Rgba8, .kind = gpu::Device::ImageKind::Texture, .levels = pixels, .label = "haylen-texture"});
}

int DeviceState::getLayers(const Device::TextureData& data) noexcept {
    return data.type == gpu::Device::TextureType::Volume ? data.depth : data.layers;
}

gpu::Device::Image DeviceState::createStagedImage(const Device::TextureData& data, bool mipmaps, int& mipLevels) {
    const gpu::Device::ImageDesc shape{.type = data.type, .width = data.width, .height = data.height, .layers = getLayers(data), .format = data.format};
    mipLevels = mipmaps ? gpu::ImageInfo::getMaxMipLevels(shape) : 1;
    return gpu->makeImage({.type = data.type, .width = data.width, .height = data.height, .layers = shape.layers, .mipLevels = mipLevels, .format = data.format, .kind = gpu::Device::ImageKind::Dynamic, .label = "haylen-dynamic-texture"});
}

// The pixels of each layer of the first level reach the image with the next upload, the way the changes of a dynamic texture do.
Texture DeviceState::createStagedTexture(const Device::TextureData& data, Texture::Options options, bool dynamic) {
    const bool mipmaps = options.mipmaps.value_or(false);
    options.mipmaps = mipmaps;
    options = settle(options);
    const int layers = getLayers(data);
    auto resource = std::make_shared<TextureResource>();
    resource->image = createStagedImage(data, mipmaps, resource->mipLevels);
    const gpu::Device::ImageDesc shape{.type = data.type, .width = data.width, .height = data.height, .layers = layers, .format = data.format};
    std::size_t bytes = 0;
    for (int level = 0; level < resource->mipLevels; ++level) {
        bytes += gpu::ImageInfo::getLevelBytes(shape, level);
    }

    resource->tracked.setBytes(bytes);
    resource->sampler = getSampler(options);
    resource->width = data.width;
    resource->height = data.height;
    resource->id = nextTextureId++;
    resource->options = options;
    resource->format = data.format;
    resource->type = data.type;
    resource->layers = layers;
    resource->dynamic = dynamic;
    resource->generatesMipmaps = mipmaps;
    resource->graveyard = graveyard;
    resource->staged.resize(static_cast<std::size_t>(layers));

    const std::size_t layerBytes = getImageBytes(data.width, data.height, data.format);
    const auto* first = reinterpret_cast<const std::uint8_t*>(data.levels.front().data());
    for (int layer = 0; layer < layers; ++layer) {
        const std::span<const std::uint8_t> pixels(first + (static_cast<std::size_t>(layer) * layerBytes), layerBytes);
        stagePixels(resource, layer, pixels, {0, 0, data.width, data.height});
    }

    return Texture(std::move(resource));
}

void DeviceState::stagePixels(const std::shared_ptr<TextureResource>& resource, int layer, std::span<const std::uint8_t> pixels, gpu::Device::Region changed) {
    TextureResource::StagedLayer& staged = resource->staged[static_cast<std::size_t>(layer)];
    if (staged.pending) {
        changed = changed.merged(staged.region);
    }

    staged.pending = true;
    if (!resource->pending) {
        resource->pending = true;
        changedTextures.push_back(resource);
    }

    const std::size_t pixelBytes = gpu::ImageInfo::getRowBytes(resource->format, 1);
    const std::size_t imageRow = gpu::ImageInfo::getRowBytes(resource->format, resource->width);
    const std::size_t regionRow = gpu::ImageInfo::getRowBytes(resource->format, changed.width);
    staged.region = changed;
    staged.pixels.resize(regionRow * static_cast<std::size_t>(changed.height));
    for (int row = 0; row < changed.height; ++row) {
        const std::size_t from = static_cast<std::size_t>(changed.y + row) * imageRow + static_cast<std::size_t>(changed.x) * pixelBytes;
        std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(from), regionRow, staged.pixels.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * regionRow));
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
