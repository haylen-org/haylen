#include "graphics/DeviceState.hpp"

#include <bit>
#include <memory>
#include <utility>

#include "graphics/Gpu.hpp"
#include "graphics/RenderTargetResource.hpp"
#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

sg_wrap DeviceState::toWrap(Texture::Wrap wrap) noexcept {
    switch (wrap) {
    case Texture::Wrap::Clamp:
        return SG_WRAP_CLAMP_TO_EDGE;
    case Texture::Wrap::Repeat:
        return SG_WRAP_REPEAT;
    case Texture::Wrap::Mirror:
        return SG_WRAP_MIRRORED_REPEAT;
    }
    return SG_WRAP_CLAMP_TO_EDGE;
}

std::uint8_t DeviceState::samplerKey(Texture::Options options) noexcept {
    return static_cast<std::uint8_t>(static_cast<unsigned>(options.filter) | (static_cast<unsigned>(options.wrap) << 1U) | (options.mipmaps ? 8U : 0U));
}

sg_sampler DeviceState::getSampler(Texture::Options options) {
    const std::uint8_t key = samplerKey(options);
    if (const auto found = samplers.find(key); found != samplers.end()) {
        return found->second;
    }

    sg_sampler_desc desc{};
    desc.min_filter = options.filter == Texture::Filter::Nearest ? SG_FILTER_NEAREST : SG_FILTER_LINEAR;
    desc.mag_filter = desc.min_filter;
    desc.mipmap_filter = options.mipmaps ? desc.min_filter : SG_FILTER_NEAREST;
    desc.wrap_u = toWrap(options.wrap);
    desc.wrap_v = toWrap(options.wrap);
    desc.label = "haylen-sampler";
    const sg_sampler sampler = sg_make_sampler(&desc);
    samplers.emplace(key, sampler);
    return sampler;
}

DeviceState::ImageViews DeviceState::createImage(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, const char* label, std::span<const Image> mipmaps) {
    sg_image_desc imageDesc{};
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.pixel_format = format;
    imageDesc.num_mipmaps = static_cast<int>(mipmaps.size()) + 1;
    imageDesc.data.mip_levels[0] = {.ptr = pixels.data(), .size = pixels.size()};
    for (std::size_t level = 0; level < mipmaps.size(); ++level) {
        imageDesc.data.mip_levels[level + 1] = {.ptr = mipmaps[level].getPixels().data(), .size = mipmaps[level].getPixels().size()};
    }
    imageDesc.label = label;

    ImageViews result;
    result.image = Gpu::makeImage(imageDesc);

    sg_view_desc viewDesc{};
    viewDesc.texture.image = result.image;
    viewDesc.label = label;
    result.view = sg_make_view(&viewDesc);
    return result;
}

// Mipmaps an image brings from its decoding are taken as they are, and the others are made here.
DeviceState::ImageViews DeviceState::createTextureImage(const Image& image, bool mipmaps, std::size_t& bytes) {
    bytes = getImageBytes(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8);
    if (!mipmaps) {
        return createImage(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), "haylen-texture");
    }
    Image levels;
    std::span<const Image> made = image.getMipmaps();
    if (made.empty() && (image.getWidth() > 1 || image.getHeight() > 1)) {
        levels = image;
        levels.buildMipmaps();
        made = levels.getMipmaps();
    }
    for (const Image& level : made) {
        bytes += level.getPixels().size();
    }
    return createImage(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), "haylen-texture", made);
}

DeviceState::ImageViews DeviceState::createDynamicImage(int width, int height, sg_pixel_format format) {
    sg_image_desc imageDesc{};
    imageDesc.usage.copy_dst = true;
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.pixel_format = format;
    imageDesc.label = "haylen-dynamic-texture";

    ImageViews result;
    result.image = Gpu::makeImage(imageDesc);
    sg_view_desc viewDesc{};
    viewDesc.texture.image = result.image;
    viewDesc.label = imageDesc.label;
    result.view = sg_make_view(&viewDesc);
    return result;
}

Texture DeviceState::createDynamicTexture(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, Texture::Options options) {
    const ImageViews created = createDynamicImage(width, height, format);
    auto resource = std::make_shared<TextureResource>();
    resource->image = created.image;
    resource->view = created.view;
    resource->tracked.setBytes(getImageBytes(width, height, format));
    resource->sampler = getSampler(options);
    resource->width = width;
    resource->height = height;
    resource->id = nextTextureId++;
    resource->options = options;
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

void DeviceState::reserveStaging(std::size_t bytes) {
    if (bytes <= stagingSize) {
        return;
    }

    if (staging.id != SG_INVALID_ID) {
        graveyard->buryBuffer(staging);
    }
    stagingSize = std::bit_ceil(bytes);
    sg_buffer_desc desc{};
    desc.size = stagingSize;
    desc.usage.staging_buffer = true;
    desc.usage.write_transient = true;
    desc.usage.copy_src = true;
    desc.label = "haylen-texture-staging";
    staging = Gpu::makeBuffer(desc);
}

std::size_t DeviceState::getStagingBytes(const TextureResource& resource) noexcept {
    return (resource.staged.size() + 3U) & ~std::size_t{3U};
}

std::size_t DeviceState::getImageBytes(int width, int height, sg_pixel_format format) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(sg_query_pixelformat(format).bytes_per_pixel);
}

RenderTarget DeviceState::createRenderTarget(int width, int height, sg_pixel_format format, Texture::Options options) {
    sg_image_desc imageDesc{};
    imageDesc.usage.color_attachment = true;
    imageDesc.usage.copy_src = true;
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.pixel_format = format;
    imageDesc.sample_count = 1;
    imageDesc.label = "haylen-render-target";

    auto texture = std::make_shared<TextureResource>(TextureResource::targets);
    texture->image = Gpu::makeImage(imageDesc);
    texture->tracked.setBytes(getImageBytes(width, height, format));

    sg_view_desc textureView{};
    textureView.texture.image = texture->image;
    texture->view = sg_make_view(&textureView);
    texture->sampler = getSampler(options);
    texture->width = width;
    texture->height = height;
    texture->id = nextTextureId++;
    texture->options = options;
    texture->flipped = !sg_query_features().origin_top_left;
    texture->graveyard = graveyard;

    auto resource = std::make_shared<RenderTargetResource>();
    sg_view_desc attachmentView{};
    attachmentView.color_attachment.image = texture->image;
    resource->attachment = sg_make_view(&attachmentView);
    resource->texture = Texture(std::move(texture));
    resource->graveyard = graveyard;
    return RenderTarget(std::move(resource));
}

} // namespace haylen::graphics
