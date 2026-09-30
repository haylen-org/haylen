#include "haylen/graphics/Device.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "graphics/DeviceSetup.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/GpuAdapter.hpp"
#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

void Device::validateSize(int width, int height, int limit) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Texture dimensions must be positive.");
    }
    if (width > limit || height > limit) {
        throw std::invalid_argument("Texture dimensions exceed the device limit.");
    }
}

void Device::validateAlpha(int width, int height, std::span<const std::uint8_t> alpha, int limit) {
    validateSize(width, height, limit);
    if (alpha.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        throw std::invalid_argument("Alpha texture data does not match its dimensions.");
    }
}

Device::Device(const DeviceSetup& setup) : state(std::make_unique<DeviceState>()) {
    // Sokol keeps one global device, so a process can run only one engine at a time.
    if (sg_isvalid()) {
        throw std::logic_error("Only one graphics device can exist at a time.");
    }

    sg_desc desc{};
    desc.environment = setup.environment;
    desc.logger = setup.logger;
    desc.disable_validation = !setup.validation;
    desc.image_pool_size = Gpu::kImagePoolSize;
    desc.view_pool_size = Gpu::kViewPoolSize;
    desc.buffer_pool_size = Gpu::kBufferPoolSize;
    desc.shader_pool_size = Gpu::kShaderPoolSize;
    desc.pipeline_pool_size = Gpu::kPipelinePoolSize;
    sg_setup(&desc);
    if (!sg_isvalid()) {
        throw std::runtime_error("The graphics device could not be initialized.");
    }
    state->white = createTexture(Image(1, 1, math::Color::white()));
}

Device::~Device() {
    state->white = {};
    state->graveyard->collect();
    state->graveyard.reset();
    for (const auto& [key, sampler] : state->samplers) {
        sg_destroy_sampler(sampler);
    }
    sg_shutdown();
}

Texture Device::createTexture(const Image& image, Texture::Options options) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());

    auto resource = std::make_shared<TextureResource>();
    const DeviceState::ImageViews created = DeviceState::createImage(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), "haylen-texture");
    resource->image = created.image;
    resource->view = created.view;
    resource->tracked.setBytes(DeviceState::getImageBytes(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8));
    resource->sampler = state->getSampler(options);
    resource->width = image.getWidth();
    resource->height = image.getHeight();
    resource->id = state->nextTextureId++;
    resource->options = options;
    resource->graveyard = state->graveyard;
    return Texture(std::move(resource));
}

Texture Device::createTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createTexture(Image(width, height, fill), options);
}

const Texture& Device::getWhiteTexture() const noexcept {
    return state->white;
}

RenderTarget Device::createRenderTarget(int width, int height, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return state->createRenderTarget(width, height, state->offscreenFormat, options);
}

void Device::replaceTexture(const Texture& texture, const Image& image) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());
    TextureResource& resource = *texture.getResource();
    if (resource.dynamic) {
        throw std::logic_error("A dynamic texture changes its pixels with updateTexture.");
    }

    // The old image is released only once the new one exists, so a failed creation leaves the texture intact.
    const DeviceState::ImageViews created = DeviceState::createImage(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), "haylen-texture");
    state->graveyard->bury(resource.image, resource.view);
    resource.image = created.image;
    resource.view = created.view;
    resource.width = image.getWidth();
    resource.height = image.getHeight();
    resource.tracked.setBytes(DeviceState::getImageBytes(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8));
}

Texture Device::createDynamicTexture(const Image& image, Texture::Options options) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());
    return state->createDynamicTexture(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), options);
}

Texture Device::createDynamicTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createDynamicTexture(Image(width, height, fill), options);
}

Texture Device::createDynamicAlphaTexture(int width, int height, std::span<const std::uint8_t> alpha, Texture::Options options) {
    validateAlpha(width, height, alpha, getMaxTextureSize());
    return state->createDynamicTexture(width, height, SG_PIXELFORMAT_R8, alpha, options);
}

void Device::updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels) {
    const std::shared_ptr<TextureResource>& resource = texture.getResource();
    if (!resource || !resource->dynamic) {
        throw std::logic_error("Only a dynamic texture changes its pixels in place.");
    }
    if (pixels.size() != DeviceState::getImageBytes(resource->width, resource->height, sg_query_image_desc(resource->image).pixel_format)) {
        throw std::invalid_argument("The pixels do not match the size of the dynamic texture.");
    }
    state->stagePixels(resource, pixels);
}

// Sokol takes one update of an image per frame, holding its whole mip level, so every texture uploads the last pixels it received.
std::size_t Device::uploadTextures() {
    std::size_t bytes = 0;
    for (const std::weak_ptr<TextureResource>& changed : std::exchange(state->changedTextures, {})) {
        const std::shared_ptr<TextureResource> resource = changed.lock();
        if (!resource) {
            continue;
        }
        sg_image_data data{};
        data.mip_levels[0] = {.ptr = resource->staged.data(), .size = resource->staged.size()};
        sg_update_image(resource->image, &data);
        bytes += resource->staged.size();
        resource->staged = {};
    }
    return bytes;
}

std::string_view Device::getBackendName() const noexcept {
    switch (sg_query_backend()) {
    case SG_BACKEND_GLCORE:
        return "glcore";
    case SG_BACKEND_GLES3:
        return "gles3";
    case SG_BACKEND_D3D11:
        return "d3d11";
    case SG_BACKEND_METAL_IOS:
    case SG_BACKEND_METAL_MACOS:
    case SG_BACKEND_METAL_SIMULATOR:
        return "metal";
    case SG_BACKEND_WGPU:
        return "webgpu";
    case SG_BACKEND_VULKAN:
        return "vulkan";
    case SG_BACKEND_DUMMY:
        return "dummy";
    }
    return "unknown";
}

std::string Device::getAdapterName() const {
    return GpuAdapter::getName();
}

int Device::getMaxTextureSize() const noexcept {
    return sg_query_limits().max_image_size_2d;
}

std::vector<Device::Pool> Device::getPools() const {
    const sg_stats stats = sg_query_stats();
    const sg_desc desc = sg_query_desc();
    return {
        {.name = "images", .used = static_cast<int>(stats.total.images.alive), .size = desc.image_pool_size}, {.name = "views", .used = static_cast<int>(stats.total.views.alive), .size = desc.view_pool_size}, {.name = "buffers", .used = static_cast<int>(stats.total.buffers.alive), .size = desc.buffer_pool_size}, {.name = "samplers", .used = static_cast<int>(stats.total.samplers.alive), .size = desc.sampler_pool_size}, {.name = "shaders", .used = static_cast<int>(stats.total.shaders.alive), .size = desc.shader_pool_size}, {.name = "pipelines", .used = static_cast<int>(stats.total.pipelines.alive), .size = desc.pipeline_pool_size},
    };
}

void Device::collectGarbage() {
    state->graveyard->collect();
}

} // namespace haylen::graphics
