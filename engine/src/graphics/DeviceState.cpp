#include "graphics/DeviceState.hpp"

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
    return static_cast<std::uint8_t>(static_cast<unsigned>(options.filter) | (static_cast<unsigned>(options.wrap) << 1U));
}

sg_sampler DeviceState::getSampler(Texture::Options options) {
    const std::uint8_t key = samplerKey(options);
    if (const auto found = samplers.find(key); found != samplers.end()) {
        return found->second;
    }

    sg_sampler_desc desc{};
    desc.min_filter = options.filter == Texture::Filter::Nearest ? SG_FILTER_NEAREST : SG_FILTER_LINEAR;
    desc.mag_filter = desc.min_filter;
    desc.wrap_u = toWrap(options.wrap);
    desc.wrap_v = toWrap(options.wrap);
    desc.label = "haylen-sampler";
    const sg_sampler sampler = sg_make_sampler(&desc);
    samplers.emplace(key, sampler);
    return sampler;
}

DeviceState::ImageViews DeviceState::createImage(int width, int height, sg_pixel_format format, std::span<const std::uint8_t> pixels, const char* label) {
    sg_image_desc imageDesc{};
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.pixel_format = format;
    imageDesc.data.mip_levels[0] = {.ptr = pixels.data(), .size = pixels.size()};
    imageDesc.label = label;

    ImageViews result;
    result.image = Gpu::makeImage(imageDesc);

    sg_view_desc viewDesc{};
    viewDesc.texture.image = result.image;
    viewDesc.label = label;
    result.view = sg_make_view(&viewDesc);
    return result;
}

std::size_t DeviceState::getImageBytes(int width, int height, sg_pixel_format format) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(sg_query_pixelformat(format).bytes_per_pixel);
}

RenderTarget DeviceState::createRenderTarget(int width, int height, sg_pixel_format format, Texture::Options options) {
    sg_image_desc imageDesc{};
    imageDesc.usage.color_attachment = true;
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
